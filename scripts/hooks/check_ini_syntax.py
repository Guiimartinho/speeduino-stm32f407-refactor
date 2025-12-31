#!/usr/bin/env python3
# =============================================================================
# CHECK INI SYNTAX - Validação de Arquivos INI TunerStudio
# =============================================================================
# SCG-ECU 2.0 - Modularização e Adaptação Speeduino para STM32F407VGT6
# Projeto Base: Speeduino (https://speeduino.com) por Josh Stewart
#
# Valida sintaxe básica de arquivos INI do TunerStudio.
#
# Uso:
#   python check_ini_syntax.py [arquivos...]
#
# =============================================================================

import sys
import re
import argparse
from pathlib import Path
from typing import List, Tuple, Optional
from dataclasses import dataclass


# =============================================================================
# CONFIGURAÇÃO
# =============================================================================

# Arquivos INI a ignorar (não são TunerStudio)
IGNORE_FILES = {
    'platformio.ini',
    '.pre-commit-config.yaml',
}


# =============================================================================
# CLASSES
# =============================================================================

@dataclass
class IniError:
    """Representa um erro de sintaxe INI."""
    file: str
    line: int
    message: str
    context: str

    def __str__(self) -> str:
        return f"{self.file}:{self.line}: {self.message}\n    {self.context}"


# =============================================================================
# VALIDAÇÃO
# =============================================================================

def validate_ini_file(file_path: str) -> List[IniError]:
    """
    Valida sintaxe de um arquivo INI.

    Args:
        file_path: Caminho do arquivo a validar

    Returns:
        Lista de erros encontrados
    """
    errors: List[IniError] = []

    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            lines = f.readlines()
    except (IOError, OSError) as e:
        return [IniError(file_path, 0, f"Could not read file: {e}", "")]

    # Estado de parsing
    current_section = None
    in_multiline = False
    brace_depth = 0
    bracket_depth = 0

    for line_num, line in enumerate(lines, 1):
        original_line = line
        line = line.strip()

        # Ignora linhas vazias e comentários
        if not line or line.startswith(';') or line.startswith('#'):
            continue

        # Verifica seção [section]
        if line.startswith('['):
            if not line.endswith(']'):
                errors.append(IniError(
                    file_path, line_num,
                    "Unclosed section bracket",
                    original_line.rstrip()
                ))
            else:
                current_section = line[1:-1]

        # Verifica balanceamento de chaves
        brace_depth += line.count('{') - line.count('}')
        if brace_depth < 0:
            errors.append(IniError(
                file_path, line_num,
                "Unmatched closing brace '}'",
                original_line.rstrip()
            ))
            brace_depth = 0

        # Verifica balanceamento de colchetes
        bracket_depth += line.count('[') - line.count(']')
        # Nota: colchetes podem aparecer em expressões, então só reporta se muito negativo
        if bracket_depth < -1:
            errors.append(IniError(
                file_path, line_num,
                "Unmatched closing bracket ']'",
                original_line.rstrip()
            ))
            bracket_depth = 0

        # Verifica aspas não fechadas (simples verificação)
        quote_count = line.count('"') - line.count('\\"')
        if quote_count % 2 != 0:
            # Pode ser multilinha, então só avisa
            if not in_multiline:
                in_multiline = True
            else:
                in_multiline = False

        # Verifica atribuições básicas (key = value)
        if '=' in line and not line.startswith('['):
            parts = line.split('=', 1)
            key = parts[0].strip()

            # Key não pode estar vazia
            if not key:
                errors.append(IniError(
                    file_path, line_num,
                    "Empty key in assignment",
                    original_line.rstrip()
                ))

            # Key não pode ter espaços (exceto em TunerStudio que permite)
            # Esta verificação é muito estrita, então comentada
            # if ' ' in key and not key.startswith('#'):
            #     errors.append(IniError(
            #         file_path, line_num,
            #         f"Key contains spaces: '{key}'",
            #         original_line.rstrip()
            #     ))

    # Verifica estado final
    if brace_depth > 0:
        errors.append(IniError(
            file_path, len(lines),
            f"Unclosed braces at end of file ({brace_depth} open)",
            ""
        ))

    return errors


def should_ignore_file(file_path: str) -> bool:
    """Verifica se o arquivo deve ser ignorado."""
    path = Path(file_path)
    return path.name in IGNORE_FILES


# =============================================================================
# MAIN
# =============================================================================

def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate INI file syntax"
    )
    parser.add_argument(
        'files',
        nargs='*',
        help="INI files to validate"
    )
    parser.add_argument(
        '--verbose', '-v',
        action='store_true',
        help="Show detailed output"
    )
    parser.add_argument(
        '--warn-only', '-w',
        action='store_true',
        help="Exit 0 even with errors"
    )

    args = parser.parse_args()

    if not args.files:
        print("No files specified.")
        return 0

    # Filtra arquivos
    files = [f for f in args.files if f.endswith('.ini') and not should_ignore_file(f)]

    if not files:
        if args.verbose:
            print("No INI files to check.")
        return 0

    # Valida arquivos
    all_errors: List[IniError] = []

    for file_path in files:
        if args.verbose:
            print(f"Checking: {file_path}")

        errors = validate_ini_file(file_path)
        all_errors.extend(errors)

    # Relatório
    if all_errors:
        print("\n" + "=" * 70)
        print("INI SYNTAX ERRORS")
        print("=" * 70)

        for error in all_errors:
            print(f"\n\033[91m{error}\033[0m")

        print("\n" + "=" * 70)
        print(f"Total errors: {len(all_errors)}")
        print("=" * 70)

        if not args.warn_only:
            return 1

    elif args.verbose:
        print(f"\nAll {len(files)} INI files valid.")

    return 0


if __name__ == '__main__':
    sys.exit(main())
