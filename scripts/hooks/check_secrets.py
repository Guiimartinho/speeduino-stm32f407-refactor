#!/usr/bin/env python3
# =============================================================================
# CHECK SECRETS - Detecção de Credentials Hardcoded
# =============================================================================
# SCG-ECU 2.0 - Modularização e Adaptação Speeduino para STM32F407VGT6
# Projeto Base: Speeduino (https://speeduino.com) por Josh Stewart
#
# Detecta senhas, API keys, tokens e outras credenciais hardcoded no código.
#
# Uso:
#   python check_secrets.py [arquivos...]
#   python check_secrets.py --all
#
# =============================================================================

import sys
import os
import re
import argparse
from pathlib import Path
from typing import List, Tuple, Dict, Set, Optional
from dataclasses import dataclass
from enum import Enum


# =============================================================================
# CONFIGURAÇÃO
# =============================================================================

# Extensões de arquivo a verificar
EXTENSIONS = {
    '.c', '.cpp', '.h', '.hpp', '.ino',  # C/C++
    '.py',                                # Python
    '.yaml', '.yml',                      # YAML
    '.json',                              # JSON
    '.ini', '.cfg', '.conf',              # Config
    '.sh', '.bash',                       # Shell
    '.env',                               # Environment
}

# Diretórios a ignorar
IGNORE_DIRS = {
    '.git',
    'build',
    '.pio',
    'docs/vw',
    'docs/bmw',
    'reference/doxygen',
    '.github/workflows/archived',
    'node_modules',
    '__pycache__',
}

# Arquivos específicos a ignorar
IGNORE_FILES = {
    'check_secrets.py',  # Este próprio arquivo
    '.pre-commit-config.yaml',  # Configuração de hooks
}


# =============================================================================
# PADRÕES DE DETECÇÃO
# =============================================================================

class SecretType(Enum):
    PASSWORD = "Password/Senha"
    API_KEY = "API Key"
    SECRET_KEY = "Secret Key"
    TOKEN = "Token"
    PRIVATE_KEY = "Private Key"
    AWS_KEY = "AWS Credentials"
    GITHUB_TOKEN = "GitHub Token"
    DATABASE_URL = "Database URL"
    GENERIC_SECRET = "Generic Secret"


@dataclass
class SecretPattern:
    """Define um padrão de detecção de secret."""
    name: str
    pattern: re.Pattern
    secret_type: SecretType
    min_length: int = 8  # Tamanho mínimo para evitar falsos positivos
    case_sensitive: bool = False


# Padrões de detecção
SECRET_PATTERNS: List[SecretPattern] = [
    # Senhas
    SecretPattern(
        name="Password assignment",
        pattern=re.compile(
            r'(?:password|passwd|pwd|pass)\s*[=:]\s*["\']([^"\']{8,})["\']',
            re.IGNORECASE
        ),
        secret_type=SecretType.PASSWORD,
    ),

    # API Keys
    SecretPattern(
        name="API Key assignment",
        pattern=re.compile(
            r'(?:api_?key|apikey|api_?secret)\s*[=:]\s*["\']([^"\']{16,})["\']',
            re.IGNORECASE
        ),
        secret_type=SecretType.API_KEY,
        min_length=16,
    ),

    # Secret Keys
    SecretPattern(
        name="Secret Key assignment",
        pattern=re.compile(
            r'(?:secret_?key|secretkey|secret)\s*[=:]\s*["\']([^"\']{16,})["\']',
            re.IGNORECASE
        ),
        secret_type=SecretType.SECRET_KEY,
        min_length=16,
    ),

    # Tokens genéricos
    SecretPattern(
        name="Token assignment",
        pattern=re.compile(
            r'(?:token|auth_?token|access_?token|bearer)\s*[=:]\s*["\']([^"\']{20,})["\']',
            re.IGNORECASE
        ),
        secret_type=SecretType.TOKEN,
        min_length=20,
    ),

    # Private Keys (PEM format)
    SecretPattern(
        name="Private Key (PEM)",
        pattern=re.compile(
            r'-----BEGIN\s+(?:RSA\s+)?PRIVATE\s+KEY-----',
            re.IGNORECASE
        ),
        secret_type=SecretType.PRIVATE_KEY,
        min_length=0,
    ),

    # AWS Credentials
    SecretPattern(
        name="AWS Access Key ID",
        pattern=re.compile(
            r'(?:aws_?access_?key_?id|AKIA)\s*[=:]\s*["\']?(AKIA[A-Z0-9]{16})["\']?',
            re.IGNORECASE
        ),
        secret_type=SecretType.AWS_KEY,
        min_length=20,
    ),
    SecretPattern(
        name="AWS Secret Access Key",
        pattern=re.compile(
            r'aws_?secret_?access_?key\s*[=:]\s*["\']([A-Za-z0-9/+=]{40})["\']',
            re.IGNORECASE
        ),
        secret_type=SecretType.AWS_KEY,
        min_length=40,
    ),

    # GitHub Tokens
    SecretPattern(
        name="GitHub Token",
        pattern=re.compile(
            r'(?:github_?token|gh_?token|ghp_)[=:\s]*["\']?(ghp_[A-Za-z0-9]{36})["\']?',
            re.IGNORECASE
        ),
        secret_type=SecretType.GITHUB_TOKEN,
        min_length=40,
    ),
    SecretPattern(
        name="GitHub Personal Access Token",
        pattern=re.compile(
            r'github_pat_[A-Za-z0-9_]{22,}',
        ),
        secret_type=SecretType.GITHUB_TOKEN,
        min_length=30,
        case_sensitive=True,
    ),

    # Database URLs
    SecretPattern(
        name="Database URL with credentials",
        pattern=re.compile(
            r'(?:mysql|postgres|mongodb|redis)://[^:]+:([^@]{8,})@',
            re.IGNORECASE
        ),
        secret_type=SecretType.DATABASE_URL,
    ),

    # Generic secrets (alta entropia)
    SecretPattern(
        name="Generic secret pattern",
        pattern=re.compile(
            r'(?:secret|credential|auth)\s*[=:]\s*["\']([A-Za-z0-9+/=]{32,})["\']',
            re.IGNORECASE
        ),
        secret_type=SecretType.GENERIC_SECRET,
        min_length=32,
    ),
]

# Padrões a ignorar (falsos positivos conhecidos)
IGNORE_PATTERNS: List[re.Pattern] = [
    # Exemplos e placeholders
    re.compile(r'your[_-]?(?:password|api[_-]?key|secret|token)', re.IGNORECASE),
    re.compile(r'<(?:password|api[_-]?key|secret|token)>', re.IGNORECASE),
    re.compile(r'(?:example|sample|test|demo|dummy|fake|placeholder)', re.IGNORECASE),
    re.compile(r'\$\{[^}]+\}'),  # Variáveis de ambiente ${VAR}
    re.compile(r'%[^%]+%'),  # Variáveis Windows %VAR%
    re.compile(r'secrets\.[A-Z_]+'),  # GitHub secrets reference

    # Valores vazios ou padrão
    re.compile(r'["\'](?:xxx+|password|changeme|123456|admin)["\']', re.IGNORECASE),

    # Comentários e documentação
    re.compile(r'#.*(?:password|secret|key)', re.IGNORECASE),
    re.compile(r'//.*(?:password|secret|key)', re.IGNORECASE),
    re.compile(r'/\*.*(?:password|secret|key).*\*/', re.IGNORECASE | re.DOTALL),
]


# =============================================================================
# CLASSES
# =============================================================================

@dataclass
class SecretFinding:
    """Representa um secret encontrado."""
    file: str
    line: int
    secret_type: SecretType
    pattern_name: str
    matched_value: str
    context: str

    def __str__(self) -> str:
        # Mascara o valor encontrado
        masked = self._mask_value(self.matched_value)
        return (
            f"{self.file}:{self.line}: {self.secret_type.value}\n"
            f"    Pattern: {self.pattern_name}\n"
            f"    Value: {masked}\n"
            f"    Context: {self.context.strip()}"
        )

    def _mask_value(self, value: str) -> str:
        """Mascara o valor do secret para exibição."""
        if len(value) <= 8:
            return '*' * len(value)
        return value[:4] + '*' * (len(value) - 8) + value[-4:]


# =============================================================================
# ANÁLISE
# =============================================================================

def should_ignore_match(line: str, match: str) -> bool:
    """Verifica se o match deve ser ignorado (falso positivo)."""
    for pattern in IGNORE_PATTERNS:
        if pattern.search(line) or pattern.search(match):
            return True
    return False


def check_file_for_secrets(file_path: str) -> List[SecretFinding]:
    """
    Analisa um arquivo em busca de secrets hardcoded.

    Args:
        file_path: Caminho do arquivo a analisar

    Returns:
        Lista de secrets encontrados
    """
    findings: List[SecretFinding] = []

    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            lines = f.readlines()
    except (IOError, OSError) as e:
        print(f"Warning: Could not read {file_path}: {e}", file=sys.stderr)
        return findings

    for line_num, line in enumerate(lines, 1):
        # Pula linhas vazias e comentários puros
        stripped = line.strip()
        if not stripped or stripped.startswith('#') or stripped.startswith('//'):
            continue

        # Verifica cada padrão
        for secret_pattern in SECRET_PATTERNS:
            matches = secret_pattern.pattern.finditer(line)

            for match in matches:
                # Extrai o valor capturado (grupo 1 se existir, senão match completo)
                try:
                    matched_value = match.group(1) if match.lastindex else match.group(0)
                except IndexError:
                    matched_value = match.group(0)

                # Verifica tamanho mínimo
                if len(matched_value) < secret_pattern.min_length:
                    continue

                # Verifica se é falso positivo
                if should_ignore_match(line, matched_value):
                    continue

                # Supressão inline: // SECRET_OK ou /* SECRET_OK */
                if 'SECRET_OK' in line or 'NOCHECK' in line:
                    continue

                findings.append(SecretFinding(
                    file=file_path,
                    line=line_num,
                    secret_type=secret_pattern.secret_type,
                    pattern_name=secret_pattern.name,
                    matched_value=matched_value,
                    context=line.rstrip(),
                ))

    return findings


def should_ignore_file(file_path: str) -> bool:
    """Verifica se o arquivo deve ser ignorado."""
    path = Path(file_path)

    # Verifica arquivos ignorados
    if path.name in IGNORE_FILES:
        return True

    # Verifica diretórios ignorados
    for ignore_dir in IGNORE_DIRS:
        if ignore_dir in str(path):
            return True

    return False


def find_files_to_check(directory: str = '.') -> List[str]:
    """Encontra todos os arquivos a verificar."""
    files = []
    for root, dirs, filenames in os.walk(directory):
        # Remove diretórios ignorados
        dirs[:] = [d for d in dirs if d not in {'.git', 'build', '.pio', '__pycache__', 'node_modules'}]

        for filename in filenames:
            if Path(filename).suffix.lower() in EXTENSIONS:
                file_path = os.path.join(root, filename)
                if not should_ignore_file(file_path):
                    files.append(file_path)

    return sorted(files)


# =============================================================================
# RELATÓRIO
# =============================================================================

def print_summary(findings: List[SecretFinding], files_checked: int) -> None:
    """Imprime resumo da análise."""
    print("\n" + "=" * 70)
    print("SECRETS CHECK SUMMARY")
    print("=" * 70)
    print(f"Files checked: {files_checked}")
    print(f"Secrets found: {len(findings)}")

    if findings:
        # Agrupa por tipo
        by_type: Dict[SecretType, int] = {}
        for f in findings:
            by_type[f.secret_type] = by_type.get(f.secret_type, 0) + 1

        print("\nBy type:")
        for secret_type, count in sorted(by_type.items(), key=lambda x: -x[1]):
            print(f"  - {secret_type.value}: {count}")

        # Agrupa por arquivo
        by_file: Dict[str, int] = {}
        for f in findings:
            by_file[f.file] = by_file.get(f.file, 0) + 1

        print(f"\nFiles with secrets: {len(by_file)}")
        for file, count in sorted(by_file.items()):
            print(f"  - {file}: {count}")

    print("=" * 70)


def print_findings(findings: List[SecretFinding]) -> None:
    """Imprime findings detalhados."""
    if not findings:
        return

    print("\n" + "-" * 70)
    print("SECRETS FOUND - ACTION REQUIRED!")
    print("-" * 70)
    print("\033[91m")  # Vermelho

    for finding in findings:
        print(f"\n{finding}")

    print("\033[0m")  # Reset
    print("\nTo suppress a finding, add '// SECRET_OK' or '/* SECRET_OK */' to the line")


# =============================================================================
# MAIN
# =============================================================================

def main() -> int:
    parser = argparse.ArgumentParser(
        description="Check for hardcoded secrets in source files"
    )
    parser.add_argument(
        'files',
        nargs='*',
        help="Files to check"
    )
    parser.add_argument(
        '--all',
        action='store_true',
        help="Check all files in project"
    )
    parser.add_argument(
        '--verbose', '-v',
        action='store_true',
        help="Show detailed output"
    )
    parser.add_argument(
        '--summary-only', '-s',
        action='store_true',
        help="Show only summary"
    )
    parser.add_argument(
        '--warn-only', '-w',
        action='store_true',
        help="Exit 0 even with findings (warning mode)"
    )

    args = parser.parse_args()

    # Determina arquivos a verificar
    if args.all:
        script_dir = Path(__file__).parent
        project_root = script_dir.parent.parent
        files = find_files_to_check(str(project_root))
    elif args.files:
        files = [f for f in args.files if Path(f).suffix.lower() in EXTENSIONS]
        files = [f for f in files if not should_ignore_file(f)]
    else:
        print("No files specified. Use --all to check all files.")
        return 0

    if not files:
        if args.verbose:
            print("No files to check.")
        return 0

    # Analisa arquivos
    all_findings: List[SecretFinding] = []

    for file_path in files:
        if args.verbose:
            print(f"Checking: {file_path}")

        findings = check_file_for_secrets(file_path)
        all_findings.extend(findings)

    # Relatório
    if not args.summary_only:
        print_findings(all_findings)

    print_summary(all_findings, len(files))

    # Código de saída
    if args.warn_only:
        return 0

    return 1 if all_findings else 0


if __name__ == '__main__':
    sys.exit(main())
