#!/usr/bin/env python3
# =============================================================================
# RUN BUILD - Verificação de Compilação do Firmware
# =============================================================================
# SCG-ECU 2.0 - Modularização e Adaptação Speeduino para STM32F407VGT6
# Projeto Base: Speeduino (https://speeduino.com) por Josh Stewart
#
# Verifica se o firmware compila corretamente.
#
# Uso:
#   python run_build.py              # Build completo
#   python run_build.py --check      # Verificação rápida (para hooks)
#   python run_build.py --clean      # Limpa e rebuilda
#
# Requisitos:
#   - PlatformIO instalado
#
# =============================================================================

import sys
import os
import subprocess
import argparse
import shutil
import time
import re
from pathlib import Path
from typing import Optional, Tuple
from dataclasses import dataclass


# =============================================================================
# CONFIGURAÇÃO
# =============================================================================

# Environment padrão para build
DEFAULT_ENV = "black_F407VE-EEPROM-SPI"

# Budgets de memória (mesmo do CI)
FLASH_BUDGET_PERCENT = 45.0  # 45% de 512KB
RAM_BUDGET_PERCENT = 20.0    # 20% de 128KB


# =============================================================================
# CLASSES
# =============================================================================

@dataclass
class BuildResult:
    """Resultado de um build."""
    success: bool
    duration: float
    flash_bytes: int
    flash_percent: float
    ram_bytes: int
    ram_percent: float
    warnings: int
    errors: int
    output: str


# =============================================================================
# FUNÇÕES
# =============================================================================

def find_platformio() -> Optional[str]:
    """Encontra o executável do PlatformIO."""
    pio = shutil.which("pio")
    if pio:
        return pio

    platformio = shutil.which("platformio")
    if platformio:
        return platformio

    common_paths = [
        os.path.expanduser("~/.platformio/penv/Scripts/platformio.exe"),
        os.path.expanduser("~/.platformio/penv/bin/platformio"),
    ]

    for path in common_paths:
        if os.path.exists(path):
            return path

    return None


def get_project_root() -> Path:
    """Retorna o diretório raiz do projeto."""
    script_dir = Path(__file__).parent
    return script_dir.parent


def parse_memory_usage(output: str) -> Tuple[int, float, int, float]:
    """
    Parse uso de memória da saída do PlatformIO.

    Returns:
        Tuple de (flash_bytes, flash_percent, ram_bytes, ram_percent)
    """
    flash_bytes = 0
    flash_percent = 0.0
    ram_bytes = 0
    ram_percent = 0.0

    # Padrão: "RAM:   [===       ]  16.3% (used 21376 bytes from 131072 bytes)"
    # Padrão: "Flash: [====      ]  37.1% (used 194420 bytes from 524288 bytes)"

    ram_pattern = r'RAM:\s+\[.*?\]\s+([\d.]+)%\s+\(used\s+(\d+)\s+bytes'
    flash_pattern = r'Flash:\s+\[.*?\]\s+([\d.]+)%\s+\(used\s+(\d+)\s+bytes'

    ram_match = re.search(ram_pattern, output)
    if ram_match:
        ram_percent = float(ram_match.group(1))
        ram_bytes = int(ram_match.group(2))

    flash_match = re.search(flash_pattern, output)
    if flash_match:
        flash_percent = float(flash_match.group(1))
        flash_bytes = int(flash_match.group(2))

    return flash_bytes, flash_percent, ram_bytes, ram_percent


def count_issues(output: str) -> Tuple[int, int]:
    """Conta warnings e erros."""
    warnings = len(re.findall(r'warning:', output, re.IGNORECASE))
    errors = len(re.findall(r'error:', output, re.IGNORECASE))
    return warnings, errors


def run_build(
    env: str = DEFAULT_ENV,
    clean: bool = False,
    verbose: bool = False,
    timeout: int = 300,
) -> BuildResult:
    """
    Executa build do firmware.

    Args:
        env: PlatformIO environment
        clean: Limpar antes do build
        verbose: Output verboso
        timeout: Timeout em segundos

    Returns:
        BuildResult com resultados
    """
    pio = find_platformio()
    if not pio:
        return BuildResult(
            success=False,
            duration=0,
            flash_bytes=0,
            flash_percent=0,
            ram_bytes=0,
            ram_percent=0,
            warnings=0,
            errors=1,
            output="PlatformIO not found",
        )

    project_root = get_project_root()

    # Limpa se solicitado
    if clean:
        clean_cmd = [pio, "run", "-e", env, "-t", "clean"]
        subprocess.run(
            clean_cmd,
            capture_output=True,
            cwd=str(project_root),
            timeout=60,
        )

    # Build
    cmd = [pio, "run", "-e", env]
    if verbose:
        cmd.append("-v")

    start = time.time()

    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            cwd=str(project_root),
            timeout=timeout,
        )
        duration = time.time() - start
        output = result.stdout + result.stderr

        # Parse resultados
        flash_bytes, flash_percent, ram_bytes, ram_percent = parse_memory_usage(output)
        warnings, errors = count_issues(output)

        return BuildResult(
            success=(result.returncode == 0),
            duration=duration,
            flash_bytes=flash_bytes,
            flash_percent=flash_percent,
            ram_bytes=ram_bytes,
            ram_percent=ram_percent,
            warnings=warnings,
            errors=errors,
            output=output,
        )

    except subprocess.TimeoutExpired:
        return BuildResult(
            success=False,
            duration=timeout,
            flash_bytes=0,
            flash_percent=0,
            ram_bytes=0,
            ram_percent=0,
            warnings=0,
            errors=1,
            output=f"Timeout after {timeout}s",
        )
    except Exception as e:
        return BuildResult(
            success=False,
            duration=0,
            flash_bytes=0,
            flash_percent=0,
            ram_bytes=0,
            ram_percent=0,
            warnings=0,
            errors=1,
            output=str(e),
        )


def check_memory_budget(result: BuildResult) -> bool:
    """Verifica se o build está dentro do budget de memória."""
    flash_ok = result.flash_percent <= FLASH_BUDGET_PERCENT
    ram_ok = result.ram_percent <= RAM_BUDGET_PERCENT
    return flash_ok and ram_ok


def print_result(result: BuildResult) -> None:
    """Imprime resultado do build."""
    status = "\033[92mSUCCESS\033[0m" if result.success else "\033[91mFAILED\033[0m"

    print("\n" + "=" * 70)
    print("BUILD RESULT")
    print("=" * 70)
    print(f"Status: {status}")
    print(f"Duration: {result.duration:.1f}s")
    print()

    # Memória
    flash_status = "[OK]" if result.flash_percent <= FLASH_BUDGET_PERCENT else "[X]"
    ram_status = "[OK]" if result.ram_percent <= RAM_BUDGET_PERCENT else "[X]"

    print("Memory Usage:")
    print(f"  Flash: {result.flash_bytes:,} bytes ({result.flash_percent:.1f}%) {flash_status} [budget: {FLASH_BUDGET_PERCENT}%]")
    print(f"  RAM:   {result.ram_bytes:,} bytes ({result.ram_percent:.1f}%) {ram_status} [budget: {RAM_BUDGET_PERCENT}%]")
    print()

    # Issues
    if result.warnings > 0 or result.errors > 0:
        print("Issues:")
        if result.errors > 0:
            print(f"  \033[91mErrors: {result.errors}\033[0m")
        if result.warnings > 0:
            print(f"  \033[93mWarnings: {result.warnings}\033[0m")
        print()

    print("=" * 70)


# =============================================================================
# MAIN
# =============================================================================

def main() -> int:
    parser = argparse.ArgumentParser(
        description="Build firmware and check memory usage"
    )
    parser.add_argument(
        '--check', '-c',
        action='store_true',
        help="Quick check mode (for pre-push hooks)"
    )
    parser.add_argument(
        '--clean',
        action='store_true',
        help="Clean build"
    )
    parser.add_argument(
        '--env', '-e',
        default=DEFAULT_ENV,
        help=f"PlatformIO environment (default: {DEFAULT_ENV})"
    )
    parser.add_argument(
        '--verbose', '-v',
        action='store_true',
        help="Verbose output"
    )
    parser.add_argument(
        '--warn-only', '-w',
        action='store_true',
        help="Exit 0 even with issues"
    )
    parser.add_argument(
        '--show-output',
        action='store_true',
        help="Show full build output"
    )

    args = parser.parse_args()

    # Verifica PlatformIO
    pio = find_platformio()
    if not pio:
        print("\033[91mError: PlatformIO not found!\033[0m")
        print("\nInstall PlatformIO:")
        print("  pip install platformio")
        return 1

    print("=" * 70)
    print("BUILD CHECK - SCG-ECU 2.0")
    print("=" * 70)
    print(f"Environment: {args.env}")
    print(f"Mode: {'Check' if args.check else 'Clean' if args.clean else 'Build'}")
    print(f"PlatformIO: {pio}")
    print()

    # Executa build
    print("Building firmware...")
    result = run_build(
        env=args.env,
        clean=args.clean,
        verbose=args.verbose,
    )

    # Output completo se solicitado
    if args.show_output:
        print("\n" + "-" * 70)
        print("BUILD OUTPUT")
        print("-" * 70)
        print(result.output)

    # Resultado
    print_result(result)

    # Verificações
    if not result.success:
        print("\033[91m[FAIL] Build failed!\033[0m")
        if not args.warn_only:
            return 1

    if not check_memory_budget(result):
        print("\033[93m[WARN] Memory budget exceeded!\033[0m")
        if not args.warn_only and args.check:
            return 1

    if result.success:
        print("\033[92m[OK] Build successful!\033[0m")

    return 0


if __name__ == '__main__':
    sys.exit(main())
