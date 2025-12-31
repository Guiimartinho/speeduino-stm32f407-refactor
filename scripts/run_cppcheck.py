#!/usr/bin/env python3
# =============================================================================
# RUN CPPCHECK - Análise Estática Local
# =============================================================================
# SCG-ECU 2.0 - Modularização e Adaptação Speeduino para STM32F407VGT6
# Projeto Base: Speeduino (https://speeduino.com) por Josh Stewart
#
# Executa cppcheck localmente com configurações otimizadas para o projeto.
#
# Uso:
#   python run_cppcheck.py              # Análise completa
#   python run_cppcheck.py --quick      # Análise rápida (só erros críticos)
#   python run_cppcheck.py --misra      # Inclui verificação MISRA
#
# Requisitos:
#   - cppcheck instalado e no PATH
#
# =============================================================================

import sys
import os
import subprocess
import argparse
import shutil
from pathlib import Path
from typing import List, Optional, Tuple
from datetime import datetime


# =============================================================================
# CONFIGURAÇÃO
# =============================================================================

# Diretório fonte a analisar
SOURCE_DIR = "speeduino"

# Diretórios a ignorar
IGNORE_DIRS = [
    "libdivide",
    "test",
    "docs",
    "reference",
    ".pio",
    "build",
]

# Supressões de falsos positivos
SUPPRESSIONS = [
    "missingIncludeSystem",      # Includes de sistema não encontrados
    "unusedFunction",            # Callbacks do Speeduino/Arduino
    "unusedStructMember",        # Membros usados via macros
    "preprocessorErrorDirective", # #error em headers
    "syntaxError:*libdivide*",   # Erros de parsing em libdivide
    "unmatchedSuppression",      # Supressões não usadas
]

# Definições de pré-processador
DEFINES = [
    "STM32F407xx",
    "USE_LIBDIVIDE",
    "BOARD_SCG_ECU_20",
    "UNIT_TEST",
]

# Includes
INCLUDE_PATHS = [
    "speeduino",
    "test/mocks",
]


# =============================================================================
# FUNÇÕES
# =============================================================================

def find_cppcheck() -> Optional[str]:
    """Encontra o executável do cppcheck."""
    # Tenta encontrar no PATH
    cppcheck = shutil.which("cppcheck")
    if cppcheck:
        return cppcheck

    # Locais comuns no Windows
    common_paths = [
        r"C:\Program Files\Cppcheck\cppcheck.exe",
        r"C:\Program Files (x86)\Cppcheck\cppcheck.exe",
        os.path.expanduser(r"~\scoop\apps\cppcheck\current\cppcheck.exe"),
        os.path.expanduser(r"~\AppData\Local\Programs\cppcheck\cppcheck.exe"),
    ]

    for path in common_paths:
        if os.path.exists(path):
            return path

    return None


def get_project_root() -> Path:
    """Retorna o diretório raiz do projeto."""
    script_dir = Path(__file__).parent
    return script_dir.parent


def build_cppcheck_command(
    quick: bool = False,
    misra: bool = False,
    output_file: Optional[str] = None,
    jobs: int = 4,
) -> List[str]:
    """Constrói o comando cppcheck."""
    cppcheck = find_cppcheck()
    if not cppcheck:
        raise RuntimeError("cppcheck not found. Please install it and add to PATH.")

    project_root = get_project_root()
    source_path = project_root / SOURCE_DIR

    cmd = [cppcheck]

    # Configuração básica
    cmd.extend([
        f"--cppcheck-build-dir={project_root / '.cppcheck_cache'}",
        f"-j{jobs}",
        "--language=c++",
        "--std=c++14",
        "--platform=native",
        "--inline-suppr",
    ])

    # Habilita verificações
    if quick:
        cmd.append("--enable=error")
    else:
        cmd.append("--enable=warning,performance,portability,style")

    # Supressões
    for supp in SUPPRESSIONS:
        cmd.append(f"--suppress={supp}")

    # Defines
    for define in DEFINES:
        cmd.append(f"-D{define}")

    # Includes
    for inc in INCLUDE_PATHS:
        inc_path = project_root / inc
        if inc_path.exists():
            cmd.append(f"-I{inc_path}")

    # Ignora diretórios
    for ignore in IGNORE_DIRS:
        ignore_path = project_root / ignore
        if ignore_path.exists():
            cmd.append(f"-i{ignore_path}")

    # MISRA
    if misra:
        misra_addon = project_root / "misra" / "addons" / "misra.py"
        if misra_addon.exists():
            cmd.append(f"--addon={misra_addon}")

    # Saída
    if output_file:
        cmd.append(f"--output-file={output_file}")

    # Template de saída
    cmd.append("--template={file}:{line}: [{severity}] {id}: {message}")

    # Diretório fonte
    cmd.append(str(source_path))

    return cmd


def run_cppcheck(
    quick: bool = False,
    misra: bool = False,
    verbose: bool = False,
) -> Tuple[int, str, str]:
    """
    Executa cppcheck.

    Returns:
        Tuple de (código de saída, stdout, stderr)
    """
    try:
        cmd = build_cppcheck_command(quick=quick, misra=misra)
    except RuntimeError as e:
        return 1, "", str(e)

    if verbose:
        print(f"Running: {' '.join(cmd)}")
        print()

    # Cria diretório de cache se não existir
    project_root = get_project_root()
    cache_dir = project_root / ".cppcheck_cache"
    cache_dir.mkdir(exist_ok=True)

    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            cwd=str(project_root),
            timeout=300,  # 5 minutos
        )
        return result.returncode, result.stdout, result.stderr
    except subprocess.TimeoutExpired:
        return 124, "", "Timeout: cppcheck took too long"
    except Exception as e:
        return 1, "", str(e)


def count_issues(output: str) -> dict:
    """Conta issues por severidade."""
    counts = {
        'error': 0,
        'warning': 0,
        'style': 0,
        'performance': 0,
        'portability': 0,
        'information': 0,
    }

    for line in output.split('\n'):
        for severity in counts:
            if f'[{severity}]' in line:
                counts[severity] += 1
                break

    return counts


def print_summary(output: str, elapsed: float) -> None:
    """Imprime resumo da análise."""
    counts = count_issues(output)
    total = sum(counts.values())

    print("\n" + "=" * 70)
    print("CPPCHECK ANALYSIS SUMMARY")
    print("=" * 70)
    print(f"Time: {elapsed:.1f}s")
    print(f"Total issues: {total}")
    print()

    for severity, count in sorted(counts.items(), key=lambda x: -x[1]):
        if count > 0:
            color = "\033[91m" if severity == 'error' else "\033[93m" if severity == 'warning' else ""
            reset = "\033[0m" if color else ""
            print(f"  {color}{severity}: {count}{reset}")

    print("=" * 70)


# =============================================================================
# MAIN
# =============================================================================

def main() -> int:
    parser = argparse.ArgumentParser(
        description="Run cppcheck static analysis"
    )
    parser.add_argument(
        '--quick', '-q',
        action='store_true',
        help="Quick analysis (errors only)"
    )
    parser.add_argument(
        '--misra', '-m',
        action='store_true',
        help="Include MISRA C:2012 checks"
    )
    parser.add_argument(
        '--verbose', '-v',
        action='store_true',
        help="Verbose output"
    )
    parser.add_argument(
        '--output', '-o',
        help="Output file for results"
    )
    parser.add_argument(
        '--warn-only', '-w',
        action='store_true',
        help="Exit 0 even with issues"
    )

    args = parser.parse_args()

    print("=" * 70)
    print("CPPCHECK STATIC ANALYSIS - SCG-ECU 2.0")
    print("=" * 70)
    print(f"Mode: {'Quick' if args.quick else 'Full'}")
    print(f"MISRA: {'Enabled' if args.misra else 'Disabled'}")
    print()

    # Verifica cppcheck
    cppcheck = find_cppcheck()
    if not cppcheck:
        print("\033[91mError: cppcheck not found!\033[0m")
        print("\nInstall cppcheck:")
        print("  Windows: winget install cppcheck")
        print("  Linux:   sudo apt install cppcheck")
        print("  macOS:   brew install cppcheck")
        return 1

    # Versão
    try:
        version = subprocess.run(
            [cppcheck, "--version"],
            capture_output=True,
            text=True
        )
        print(f"Using: {version.stdout.strip()}")
    except Exception:
        pass

    print()

    # Executa análise
    import time
    start = time.time()

    returncode, stdout, stderr = run_cppcheck(
        quick=args.quick,
        misra=args.misra,
        verbose=args.verbose,
    )

    elapsed = time.time() - start

    # Output
    output = stderr  # cppcheck usa stderr para warnings

    if output:
        print(output)

    if args.output:
        with open(args.output, 'w') as f:
            f.write(output)
        print(f"\nResults saved to: {args.output}")

    print_summary(output, elapsed)

    # Código de saída
    if args.warn_only:
        return 0

    counts = count_issues(output)
    if counts['error'] > 0:
        return 1

    return 0


if __name__ == '__main__':
    sys.exit(main())
