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
from typing import List, Dict, Optional
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
    'backups',
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


class ParseState(Enum):
    """Estado do parser."""
    CODE = 0
    LINE_COMMENT = 1
    BLOCK_COMMENT = 2
    STRING_DOUBLE = 3
    STRING_SINGLE = 4
    CHAR_LITERAL = 5


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

def check_nesting_depth(
    file_path: str,
    max_depth: int = DEFAULT_MAX_DEPTH
) -> List[NestingViolation]:
    """
    Analisa um arquivo e retorna violações de profundidade de aninhamento.

    Usa um parser de estado para ignorar comentários e strings corretamente.

    Args:
        file_path: Caminho do arquivo a analisar
        max_depth: Profundidade máxima permitida

    Returns:
        Lista de violações encontradas
    """
    violations = []

    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
    except (IOError, OSError) as e:
        print(f"Warning: Could not read {file_path}: {e}", file=sys.stderr)
        return violations

    lines = content.split('\n')
    state = ParseState.CODE
    depth = 0
    line_num = 1
    i = 0
    n = len(content)

    # Set para evitar duplicatas na mesma linha
    reported_lines: set = set()

    while i < n:
        char = content[i]
        next_char = content[i + 1] if i + 1 < n else ''

        # Atualiza número da linha
        if char == '\n':
            line_num += 1
            # Sai de comentário de linha no final da linha
            if state == ParseState.LINE_COMMENT:
                state = ParseState.CODE
            i += 1
            continue

        # Estado: CÓDIGO
        if state == ParseState.CODE:
            # Início de comentário de bloco
            if char == '/' and next_char == '*':
                state = ParseState.BLOCK_COMMENT
                i += 2
                continue
            # Início de comentário de linha
            if char == '/' and next_char == '/':
                state = ParseState.LINE_COMMENT
                i += 2
                continue
            # Início de string
            if char == '"':
                state = ParseState.STRING_DOUBLE
                i += 1
                continue
            # Início de char literal
            if char == "'":
                state = ParseState.CHAR_LITERAL
                i += 1
                continue

            # Conta chaves apenas em código
            if char == '{':
                depth += 1

                # Verifica violação
                if depth > max_depth and line_num not in reported_lines:
                    # Verifica supressão inline
                    line_content = lines[line_num - 1] if line_num <= len(lines) else ''
                    if not SUPPRESSION_PATTERN.search(line_content):
                        # Contexto: linhas ao redor
                        context_start = max(0, line_num - 2)
                        context_end = min(len(lines), line_num + 1)
                        context_lines = lines[context_start:context_end]
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
                        reported_lines.add(line_num)

            elif char == '}':
                depth = max(0, depth - 1)

        # Estado: COMENTÁRIO DE BLOCO
        elif state == ParseState.BLOCK_COMMENT:
            if char == '*' and next_char == '/':
                state = ParseState.CODE
                i += 2
                continue

        # Estado: COMENTÁRIO DE LINHA
        elif state == ParseState.LINE_COMMENT:
            # Continua até newline (tratado acima)
            pass

        # Estado: STRING COM ASPAS DUPLAS
        elif state == ParseState.STRING_DOUBLE:
            if char == '\\' and next_char:
                # Escape - pula próximo caractere
                i += 2
                continue
            if char == '"':
                state = ParseState.CODE

        # Estado: CHAR LITERAL
        elif state == ParseState.CHAR_LITERAL:
            if char == '\\' and next_char:
                # Escape - pula próximo caractere
                i += 2
                continue
            if char == "'":
                state = ParseState.CODE

        i += 1

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

    # Ignora arquivos .backup
    if '.backup' in path.name:
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
        # Handle encoding errors for Windows
        try:
            print(f"\n{color}{v}{reset}")
        except UnicodeEncodeError:
            safe_str = str(v).encode('ascii', errors='replace').decode('ascii')
            print(f"\n{color}{safe_str}{reset}")


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
