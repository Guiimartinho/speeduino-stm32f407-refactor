#!/usr/bin/env python3
# =============================================================================
# RUN TESTS - Executor de Testes Unitários
# =============================================================================
# SCG-ECU 2.0 - Modularização e Adaptação Speeduino para STM32F407VGT6
# Projeto Base: Speeduino (https://speeduino.com) por Josh Stewart
#
# Executa testes unitários via PlatformIO.
#
# Uso:
#   python run_tests.py              # Todos os testes
#   python run_tests.py --quick      # Testes rápidos (core apenas)
#   python run_tests.py --suite X    # Suite específica
#   python run_tests.py --list       # Lista suites disponíveis
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
from pathlib import Path
from typing import List, Optional, Tuple
from dataclasses import dataclass


# =============================================================================
# CONFIGURAÇÃO
# =============================================================================

# Environment de testes
TEST_ENV = "native"

# Suites de teste por prioridade
TEST_SUITES = {
    # Core (críticos) - executados em --quick
    "core": [
        "test_refactored_helpers",
        "test_corrections_massive",
        "test_scheduling_massive",
    ],

    # Módulos principais
    "main": [
        "test_decoders_massive",
        "test_sensors_massive",
        "test_idle_massive",
        "test_engineProtection_massive",
    ],

    # Matemática e utilitários
    "math": [
        "test_math",
        "test_tables",
    ],

    # Sistema
    "system": [
        "test_fuel",
        "test_ign",
        "test_secondary",
    ],

    # Todos
    "all": None,  # Indica execução completa
}

# Suites para --quick (pré-push check)
QUICK_SUITES = ["test_refactored_helpers", "test_corrections_massive"]


# =============================================================================
# CLASSES
# =============================================================================

@dataclass
class TestResult:
    """Resultado de uma suite de testes."""
    suite: str
    passed: int
    failed: int
    ignored: int
    duration: float
    success: bool
    output: str


# =============================================================================
# FUNÇÕES
# =============================================================================

def find_platformio() -> Optional[str]:
    """Encontra o executável do PlatformIO."""
    # Tenta 'pio' no PATH
    pio = shutil.which("pio")
    if pio:
        return pio

    # Tenta 'platformio' no PATH
    platformio = shutil.which("platformio")
    if platformio:
        return platformio

    # Locais comuns
    common_paths = [
        os.path.expanduser("~/.platformio/penv/Scripts/platformio.exe"),
        os.path.expanduser("~/.platformio/penv/bin/platformio"),
        "/usr/local/bin/platformio",
    ]

    for path in common_paths:
        if os.path.exists(path):
            return path

    return None


def get_project_root() -> Path:
    """Retorna o diretório raiz do projeto."""
    script_dir = Path(__file__).parent
    return script_dir.parent


def list_test_suites() -> List[str]:
    """Lista todas as suites de teste disponíveis."""
    project_root = get_project_root()
    test_dir = project_root / "test"

    if not test_dir.exists():
        return []

    suites = []
    for item in test_dir.iterdir():
        if item.is_dir() and item.name.startswith("test_"):
            suites.append(item.name)

    return sorted(suites)


def run_test_suite(
    suite: str,
    verbose: bool = False,
    timeout: int = 120,
) -> TestResult:
    """
    Executa uma suite de testes.

    Args:
        suite: Nome da suite (ou None para todas)
        verbose: Output verboso
        timeout: Timeout em segundos

    Returns:
        TestResult com resultados
    """
    pio = find_platformio()
    if not pio:
        return TestResult(
            suite=suite,
            passed=0,
            failed=1,
            ignored=0,
            duration=0,
            success=False,
            output="PlatformIO not found",
        )

    project_root = get_project_root()

    cmd = [pio, "test", "-e", TEST_ENV]

    if suite:
        cmd.extend(["-f", suite])

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
        passed, failed, ignored = parse_test_output(output)

        return TestResult(
            suite=suite or "all",
            passed=passed,
            failed=failed,
            ignored=ignored,
            duration=duration,
            success=(result.returncode == 0),
            output=output,
        )

    except subprocess.TimeoutExpired:
        return TestResult(
            suite=suite or "all",
            passed=0,
            failed=1,
            ignored=0,
            duration=timeout,
            success=False,
            output=f"Timeout after {timeout}s",
        )
    except Exception as e:
        return TestResult(
            suite=suite or "all",
            passed=0,
            failed=1,
            ignored=0,
            duration=0,
            success=False,
            output=str(e),
        )


def parse_test_output(output: str) -> Tuple[int, int, int]:
    """
    Parse saída do PlatformIO para extrair contagens.

    Returns:
        Tuple de (passed, failed, ignored)
    """
    import re

    passed = 0
    failed = 0
    ignored = 0

    # Padrão: "X Tests Y Failures Z Ignored"
    pattern = r'(\d+)\s+Tests?\s+(\d+)\s+Failures?\s+(\d+)\s+Ignored'
    matches = re.findall(pattern, output)

    for match in matches:
        tests, failures, ign = map(int, match)
        passed += tests - failures - ign
        failed += failures
        ignored += ign

    # Conta [PASSED] e [FAILED] individuais se não encontrou summary
    if passed == 0 and failed == 0:
        passed = output.count('[PASSED]')
        failed = output.count('[FAILED]')

    return passed, failed, ignored


def run_quick_tests(verbose: bool = False) -> List[TestResult]:
    """Executa testes rápidos (para pre-push)."""
    results = []

    for suite in QUICK_SUITES:
        print(f"Running: {suite}...")
        result = run_test_suite(suite, verbose=verbose, timeout=60)
        results.append(result)

        if not result.success:
            break  # Para no primeiro erro

    return results


def run_all_tests(verbose: bool = False) -> TestResult:
    """Executa todos os testes."""
    return run_test_suite(None, verbose=verbose, timeout=300)


# =============================================================================
# RELATÓRIO
# =============================================================================

def print_result(result: TestResult) -> None:
    """Imprime resultado de uma suite."""
    status = "\033[92mPASS\033[0m" if result.success else "\033[91mFAIL\033[0m"

    print(f"\n{'-' * 60}")
    print(f"Suite: {result.suite}")
    print(f"Status: {status}")
    print(f"Passed: {result.passed}")
    print(f"Failed: {result.failed}")
    print(f"Ignored: {result.ignored}")
    print(f"Duration: {result.duration:.1f}s")

    if not result.success and result.output:
        print(f"\nOutput:\n{result.output[:2000]}")


def print_summary(results: List[TestResult]) -> None:
    """Imprime resumo de todos os resultados."""
    total_passed = sum(r.passed for r in results)
    total_failed = sum(r.failed for r in results)
    total_ignored = sum(r.ignored for r in results)
    total_duration = sum(r.duration for r in results)
    all_success = all(r.success for r in results)

    print("\n" + "=" * 70)
    print("TEST SUMMARY")
    print("=" * 70)
    print(f"Suites run: {len(results)}")
    print(f"Total passed: {total_passed}")
    print(f"Total failed: {total_failed}")
    print(f"Total ignored: {total_ignored}")
    print(f"Total duration: {total_duration:.1f}s")
    print()

    status = "\033[92m✓ ALL TESTS PASSED\033[0m" if all_success else "\033[91m✗ TESTS FAILED\033[0m"
    print(status)
    print("=" * 70)


# =============================================================================
# MAIN
# =============================================================================

def main() -> int:
    parser = argparse.ArgumentParser(
        description="Run unit tests via PlatformIO"
    )
    parser.add_argument(
        '--quick', '-q',
        action='store_true',
        help="Run quick tests only (core suites)"
    )
    parser.add_argument(
        '--suite', '-s',
        help="Run specific test suite"
    )
    parser.add_argument(
        '--list', '-l',
        action='store_true',
        help="List available test suites"
    )
    parser.add_argument(
        '--verbose', '-v',
        action='store_true',
        help="Verbose output"
    )
    parser.add_argument(
        '--warn-only', '-w',
        action='store_true',
        help="Exit 0 even with failures"
    )

    args = parser.parse_args()

    # Lista suites
    if args.list:
        print("Available test suites:")
        for suite in list_test_suites():
            print(f"  - {suite}")
        return 0

    # Verifica PlatformIO
    pio = find_platformio()
    if not pio:
        print("\033[91mError: PlatformIO not found!\033[0m")
        print("\nInstall PlatformIO:")
        print("  pip install platformio")
        print("  or: https://platformio.org/install")
        return 1

    print("=" * 70)
    print("UNIT TESTS - SCG-ECU 2.0")
    print("=" * 70)
    print(f"Mode: {'Quick' if args.quick else 'Suite' if args.suite else 'Full'}")
    print(f"PlatformIO: {pio}")
    print()

    # Executa testes
    results: List[TestResult] = []

    if args.suite:
        print(f"Running suite: {args.suite}")
        result = run_test_suite(args.suite, verbose=args.verbose)
        results.append(result)
        print_result(result)

    elif args.quick:
        print("Running quick tests (pre-push check)...")
        results = run_quick_tests(verbose=args.verbose)
        for result in results:
            print_result(result)

    else:
        print("Running all tests...")
        result = run_all_tests(verbose=args.verbose)
        results.append(result)
        print_result(result)

    # Resumo
    print_summary(results)

    # Código de saída
    if args.warn_only:
        return 0

    return 0 if all(r.success for r in results) else 1


if __name__ == '__main__':
    sys.exit(main())
