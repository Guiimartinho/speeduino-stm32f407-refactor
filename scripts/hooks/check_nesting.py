#!/usr/bin/env python3
# =============================================================================
# CHECK NESTING DEPTH - MISRA C:2012 Rule 15.6
# =============================================================================
# SCG-ECU 2.0 - Modularização e Adaptação Speeduino para STM32F407VGT6
# Projeto Base: Speeduino (https://speeduino.com) por Josh Stewart
#
# Verifica profundidade máxima de aninhamento em arquivos C/C++
# Limite: 3 níveis (mais estrito que MISRA padrão de 4)
#
# Uso:
#   python check_nesting.py [arquivos...]
#   python check_nesting.py --all
#   python check_nesting.py --max-depth 4
#
# =============================================================================

import sys
import os
import re
import argparse
from pathlib import Path
from typing import List, Tuple, Dict, Optional
from dataclasses import dataclass
from enum import Enum


# =============================================================================
# CONFIGURAÇÃO
# =============================================================================

DEFAULT_MAX_DEPTH = 3  # MISRA-C estrito para SCG-ECU 2.0
EXTENSIONS = {'.c', '.cpp', '.h', '.hpp', '.ino'}

# Diretórios a ignorar
IGNORE_DIRS = {
    'libdivide',
    'test/mocks',
    'docs',
    'reference',
    '.git',
    'build',
    '.pio',
}

# Arquivos específicos a ignorar (padrões conhecidos com nesting alto)
IGNORE_FILES = {
    'decoders.cpp',  # Decoders têm switch aninhados por design
}

# Supressões inline: // NESTING_OK ou /* NESTING_OK */
SUPPRESSION_PATTERN = re.compile(r'(?://|/\*)\s*NESTING_OK')


# =============================================================================
# CLASSES
# =============================================================================

class Severity(Enum):
    WARNING = "WARNING"
    ERROR = "ERROR"


@dataclass
class NestingViolation:
    """Representa uma violação de profundidade de aninhamento."""
    file: str
    line: int
    depth: int
    max_allowed: int
    context: str
    severity: Severity

    def __str__(self) -> str:
        return (
            f"{self.file}:{self.line}: {self.severity.value}: "
            f"Nesting depth {self.depth} exceeds maximum {self.max_allowed}\n"
            f"    {self.context.strip()}"
        )


# =============================================================================
# ANÁLISE DE NESTING
# =============================================================================

def remove_strings_and_comments(content: str) -> str:
    """Remove strings e comentários para análise precisa."""
    # Remove comentários de bloco /* ... */
    content = re.sub(r'/\*.*?\*/', '', content, flags=re.DOTALL)
    # Remove comentários de linha // ...
    content = re.sub(r'//.*$', '', content, flags=re.MULTILINE)
    # Remove strings "..." e '...'
    content = re.sub(r'"(?:[^"\\]|\\.)*"', '""', content)
    content = re.sub(r"'(?:[^'\\]|\\.)*'", "''", content)
    return content


def get_line_number(content: str, position: int) -> int:
    """Retorna o número da linha para uma posição no conteúdo."""
    return content[:position].count('\n') + 1


def check_nesting_depth(
    file_path: str,
    max_depth: int = DEFAULT_MAX_DEPTH,
    original_content: Optional[str] = None
) -> List[NestingViolation]:
    """
    Analisa um arquivo e retorna violações de profundidade de aninhamento.

    Args:
        file_path: Caminho do arquivo a analisar
        max_depth: Profundidade máxima permitida
        original_content: Conteúdo original (para contexto)

    Returns:
        Lista de violações encontradas
    """
    violations = []

    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            original = f.read()
    except (IOError, OSError) as e:
        print(f"Warning: Could not read {file_path}: {e}", file=sys.stderr)
        return violations

    # Guarda original para contexto
    if original_content is None:
        original_content = original

    original_lines = original_content.split('\n')

    # Remove strings e comentários para análise
    cleaned = remove_strings_and_comments(original)

    # Rastreia profundidade de nesting
    depth = 0
    max_found = 0
    position = 0

    # Mapa de posição para profundidade
    depth_at_position: Dict[int, int] = {}

    for i, char in enumerate(cleaned):
        if char == '{':
            depth += 1
            depth_at_position[i] = depth
            if depth > max_found:
                max_found = depth

            # Verifica violação
            if depth > max_depth:
                line_num = get_line_number(original, i)

                # Verifica supressão inline
                if line_num <= len(original_lines):
                    line_content = original_lines[line_num - 1]
                    if SUPPRESSION_PATTERN.search(line_content):
                        continue  # Suprimido

                # Contexto: linha atual + algumas anteriores
                context_start = max(0, line_num - 2)
                context_end = min(len(original_lines), line_num + 1)
                context_lines = original_lines[context_start:context_end]
                context = '\n    '.join(context_lines)

                severity = Severity.ERROR if depth > max_depth + 1 else Severity.WARNING

                violations.append(NestingViolation(
                    file=file_path,
                    line=line_num,
                    depth=depth,
                    max_allowed=max_depth,
                    context=context,
                    severity=severity
                ))

        elif char == '}':
            depth = max(0, depth - 1)

    return violations


def should_ignore_file(file_path: str) -> bool:
    """Verifica se o arquivo deve ser ignorado."""
    path = Path(file_path)

    # Verifica diretórios ignorados
    for part in path.parts:
        if part in IGNORE_DIRS:
            return True

    # Verifica arquivos específicos ignorados
    if path.name in IGNORE_FILES:
        return True

    return False


def find_source_files(directory: str = '.') -> List[str]:
    """Encontra todos os arquivos fonte no diretório."""
    files = []
    for root, dirs, filenames in os.walk(directory):
        # Remove diretórios ignorados
        dirs[:] = [d for d in dirs if d not in IGNORE_DIRS]

        for filename in filenames:
            if Path(filename).suffix.lower() in EXTENSIONS:
                file_path = os.path.join(root, filename)
                if not should_ignore_file(file_path):
                    files.append(file_path)

    return sorted(files)


# =============================================================================
# RELATÓRIO
# =============================================================================

def print_summary(violations: List[NestingViolation], files_checked: int) -> None:
    """Imprime resumo da análise."""
    print("\n" + "=" * 70)
    print("NESTING DEPTH CHECK SUMMARY")
    print("=" * 70)
    print(f"Files checked: {files_checked}")
    print(f"Violations found: {len(violations)}")

    if violations:
        errors = sum(1 for v in violations if v.severity == Severity.ERROR)
        warnings = sum(1 for v in violations if v.severity == Severity.WARNING)
        print(f"  - Errors: {errors}")
        print(f"  - Warnings: {warnings}")

        # Agrupa por arquivo
        by_file: Dict[str, List[NestingViolation]] = {}
        for v in violations:
            by_file.setdefault(v.file, []).append(v)

        print(f"\nFiles with violations: {len(by_file)}")
        for file, file_violations in sorted(by_file.items()):
            max_depth = max(v.depth for v in file_violations)
            print(f"  - {file}: max depth {max_depth} ({len(file_violations)} violations)")

    print("=" * 70)


def print_violations(violations: List[NestingViolation]) -> None:
    """Imprime violações detalhadas."""
    if not violations:
        return

    print("\n" + "-" * 70)
    print("VIOLATIONS DETAIL")
    print("-" * 70)

    for v in violations:
        color = "\033[91m" if v.severity == Severity.ERROR else "\033[93m"
        reset = "\033[0m"
        print(f"\n{color}{v}{reset}")


# =============================================================================
# MAIN
# =============================================================================

def main() -> int:
    parser = argparse.ArgumentParser(
        description="Check nesting depth in C/C++ files (MISRA C:2012 Rule 15.6)"
    )
    parser.add_argument(
        'files',
        nargs='*',
        help="Files to check (if empty, checks staged files or --all)"
    )
    parser.add_argument(
        '--all',
        action='store_true',
        help="Check all source files in speeduino/"
    )
    parser.add_argument(
        '--max-depth', '-d',
        type=int,
        default=DEFAULT_MAX_DEPTH,
        help=f"Maximum allowed nesting depth (default: {DEFAULT_MAX_DEPTH})"
    )
    parser.add_argument(
        '--verbose', '-v',
        action='store_true',
        help="Show detailed output"
    )
    parser.add_argument(
        '--summary-only', '-s',
        action='store_true',
        help="Show only summary, not individual violations"
    )
    parser.add_argument(
        '--warn-only', '-w',
        action='store_true',
        help="Exit 0 even with violations (warning mode)"
    )

    args = parser.parse_args()

    # Determina arquivos a verificar
    if args.all:
        # Encontra raiz do projeto
        script_dir = Path(__file__).parent
        project_root = script_dir.parent.parent
        speeduino_dir = project_root / 'speeduino'

        if speeduino_dir.exists():
            files = find_source_files(str(speeduino_dir))
        else:
            files = find_source_files('.')
    elif args.files:
        files = [f for f in args.files if Path(f).suffix.lower() in EXTENSIONS]
        files = [f for f in files if not should_ignore_file(f)]
    else:
        # Sem arquivos especificados - verifica se há arquivos passados pelo pre-commit
        print("No files specified. Use --all to check all files.")
        return 0

    if not files:
        if args.verbose:
            print("No files to check.")
        return 0

    # Analisa arquivos
    all_violations: List[NestingViolation] = []

    for file_path in files:
        if args.verbose:
            print(f"Checking: {file_path}")

        violations = check_nesting_depth(file_path, args.max_depth)
        all_violations.extend(violations)

    # Relatório
    if not args.summary_only:
        print_violations(all_violations)

    print_summary(all_violations, len(files))

    # Código de saída
    if args.warn_only:
        return 0

    # Falha apenas em erros (depth > max + 1)
    errors = [v for v in all_violations if v.severity == Severity.ERROR]
    return 1 if errors else 0


if __name__ == '__main__':
    sys.exit(main())
