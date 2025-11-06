#!/usr/bin/env python3
"""
Analyze decoders.cpp to identify all ACTIVE functions (outside #if 0 blocks)
and calculate MISRA metrics for each function.
"""

import re
from typing import List, Dict, Tuple, Optional
from dataclasses import dataclass
from enum import Enum

class Severity(Enum):
    CRITICAL = "CRITICAL"
    HIGH = "HIGH"
    MEDIUM = "MEDIUM"
    LOW = "LOW"

@dataclass
class Function:
    name: str
    start_line: int
    end_line: int
    lines: int
    nesting: int
    complexity: int
    severity: Optional[Severity] = None
    violations: List[str] = None

    def __post_init__(self):
        if self.violations is None:
            self.violations = []

def find_disabled_blocks(lines: List[str]) -> List[Tuple[int, int]]:
    """Find all #if 0 blocks and return their start and end line numbers."""
    disabled_blocks = []
    stack = []

    for i, line in enumerate(lines, 1):
        stripped = line.strip()

        # Track #if 0 blocks
        if stripped.startswith('#if 0') or stripped.startswith('#if  0'):
            stack.append(i)
        elif stripped.startswith('#endif'):
            if stack:
                start = stack.pop()
                disabled_blocks.append((start, i))

    return disabled_blocks

def is_line_active(line_num: int, disabled_blocks: List[Tuple[int, int]]) -> bool:
    """Check if a line is active (not inside any #if 0 block)."""
    for start, end in disabled_blocks:
        if start <= line_num <= end:
            return False
    return True

def calculate_nesting_level(line: str) -> int:
    """Calculate nesting level based on braces."""
    opening = line.count('{')
    closing = line.count('}')
    return opening - closing

def calculate_complexity(function_body: List[str]) -> int:
    """
    Calculate cyclomatic complexity.
    C = 1 + number of decision points (if, while, for, case, &&, ||, ?, etc.)
    """
    complexity = 1

    for line in function_body:
        # Remove comments and strings to avoid false positives
        cleaned = re.sub(r'//.*', '', line)
        cleaned = re.sub(r'/\*.*?\*/', '', cleaned)
        cleaned = re.sub(r'"([^"\\]|\\.)*"', '', cleaned)
        cleaned = re.sub(r"'([^'\\]|\\.)*'", '', cleaned)

        # Count decision points
        complexity += len(re.findall(r'\bif\b', cleaned))
        complexity += len(re.findall(r'\bwhile\b', cleaned))
        complexity += len(re.findall(r'\bfor\b', cleaned))
        complexity += len(re.findall(r'\bcase\b', cleaned))
        complexity += len(re.findall(r'\bcatch\b', cleaned))
        complexity += len(re.findall(r'\&\&', cleaned))
        complexity += len(re.findall(r'\|\|', cleaned))
        complexity += len(re.findall(r'\?', cleaned))

    return complexity

def find_function_end(lines: List[str], start_idx: int, disabled_blocks: List[Tuple[int, int]]) -> int:
    """Find the end of a function by matching braces, considering only active lines."""
    brace_count = 0
    in_function = False

    for i in range(start_idx, len(lines)):
        line_num = i + 1

        # Skip disabled blocks
        if not is_line_active(line_num, disabled_blocks):
            continue

        line = lines[i]

        # Skip comments and strings
        cleaned = re.sub(r'//.*', '', line)
        cleaned = re.sub(r'/\*.*?\*/', '', cleaned)
        cleaned = re.sub(r'"([^"\\]|\\.)*"', '', cleaned)
        cleaned = re.sub(r"'([^'\\]|\\.)*'", '', cleaned)

        for char in cleaned:
            if char == '{':
                brace_count += 1
                in_function = True
            elif char == '}':
                brace_count -= 1
                if in_function and brace_count == 0:
                    return i

    return len(lines) - 1

def extract_function_name(signature: str) -> Optional[str]:
    """Extract function name from signature."""
    # Remove return type, templates, and get function name
    # Pattern: type name(params) or type name(params) const

    # Remove inline, static, const qualifiers
    signature = re.sub(r'\binline\b', '', signature)
    signature = re.sub(r'\bstatic\b', '', signature)
    signature = re.sub(r'\bvirtual\b', '', signature)
    signature = re.sub(r'\bexplicit\b', '', signature)

    # Look for function pattern: name followed by (
    match = re.search(r'\b([a-zA-Z_][a-zA-Z0-9_]*)\s*\(', signature)
    if match:
        return match.group(1)

    return None

def find_active_functions(file_path: str) -> Tuple[List[Function], Dict]:
    """Find all active functions and calculate metrics."""
    with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
        lines = f.readlines()

    disabled_blocks = find_disabled_blocks(lines)
    functions = []

    # Stats
    total_lines = len(lines)
    disabled_lines = sum(end - start + 1 for start, end in disabled_blocks)
    active_lines = total_lines - disabled_lines

    # Function patterns
    function_pattern = re.compile(
        r'^\s*(?:inline\s+)?(?:static\s+)?(?:void|int|uint8_t|uint16_t|uint32_t|bool|unsigned|signed|long|short|char|float|double|auto)\s+'
        r'([a-zA-Z_][a-zA-Z0-9_]*)\s*\('
    )

    i = 0
    while i < len(lines):
        line_num = i + 1

        # Skip disabled blocks
        if not is_line_active(line_num, disabled_blocks):
            i += 1
            continue

        line = lines[i]

        # Check if this looks like a function definition
        match = function_pattern.match(line)
        if match:
            # Check if it's a function definition (has opening brace)
            # Look ahead for opening brace
            temp_signature = line
            j = i + 1
            found_brace = '{' in line

            # Collect full signature
            while j < len(lines) and not found_brace:
                if not is_line_active(j + 1, disabled_blocks):
                    j += 1
                    continue

                temp_signature += lines[j]
                if '{' in lines[j]:
                    found_brace = True
                    break
                if ';' in lines[j]:
                    # Declaration, not definition
                    break
                j += 1

            if found_brace:
                func_name = extract_function_name(temp_signature)
                if func_name:
                    start_line = line_num
                    end_idx = find_function_end(lines, i, disabled_blocks)
                    end_line = end_idx + 1

                    # Get function body (only active lines)
                    body_lines = []
                    max_nesting = 0
                    current_nesting = 0

                    for k in range(i, end_idx + 1):
                        if is_line_active(k + 1, disabled_blocks):
                            body_lines.append(lines[k])
                            current_nesting += calculate_nesting_level(lines[k])
                            max_nesting = max(max_nesting, current_nesting)

                    complexity = calculate_complexity(body_lines)

                    func = Function(
                        name=func_name,
                        start_line=start_line,
                        end_line=end_line,
                        lines=len(body_lines),
                        nesting=max_nesting,
                        complexity=complexity
                    )

                    # Determine violations and severity
                    violations = []
                    severity = None

                    if func.nesting >= 5:
                        violations.append(f"N={func.nesting} (CRITICAL: >=5)")
                        severity = Severity.CRITICAL
                    elif func.nesting == 4:
                        violations.append(f"N={func.nesting}")
                        severity = Severity.MEDIUM
                    elif func.nesting == 3:
                        violations.append(f"N={func.nesting}")
                        if severity is None:
                            severity = Severity.LOW

                    if func.complexity > 15:
                        violations.append(f"C={func.complexity} (HIGH: >15)")
                        if severity != Severity.CRITICAL:
                            severity = Severity.HIGH
                    elif func.complexity > 10:
                        violations.append(f"C={func.complexity}")
                        if severity is None or severity == Severity.LOW:
                            severity = Severity.MEDIUM

                    if func.lines > 100:
                        violations.append(f"Lines={func.lines} (HIGH: >100)")
                        if severity != Severity.CRITICAL:
                            severity = Severity.HIGH
                    elif func.lines > 50:
                        violations.append(f"Lines={func.lines}")
                        if severity is None or severity == Severity.LOW:
                            severity = Severity.MEDIUM
                    elif func.lines > 40:
                        violations.append(f"Lines={func.lines}")
                        if severity is None:
                            severity = Severity.LOW

                    func.violations = violations
                    func.severity = severity

                    functions.append(func)
                    i = end_idx + 1
                    continue

        i += 1

    stats = {
        'total_lines': total_lines,
        'disabled_lines': disabled_lines,
        'active_lines': active_lines,
        'disabled_blocks': disabled_blocks,
        'total_functions': len(functions)
    }

    return functions, stats

def print_report(functions: List[Function], stats: Dict):
    """Print comprehensive report."""
    print("=" * 80)
    print("ANALISE DE FUNCOES ATIVAS - speeduino/decoders.cpp")
    print("=" * 80)
    print()

    print("RESUMO DO ARQUIVO:")
    print(f"  Total de linhas:        {stats['total_lines']:,}")
    print(f"  Linhas desabilitadas:   {stats['disabled_lines']:,} ({stats['disabled_lines']/stats['total_lines']*100:.1f}%)")
    print(f"  Linhas ativas:          {stats['active_lines']:,} ({stats['active_lines']/stats['total_lines']*100:.1f}%)")
    print(f"  Blocos #if 0:           {len(stats['disabled_blocks'])}")
    print(f"  Funções ativas:         {stats['total_functions']}")
    print()

    # Count violations by severity
    violations = {
        Severity.CRITICAL: [],
        Severity.HIGH: [],
        Severity.MEDIUM: [],
        Severity.LOW: [],
        None: []
    }

    for func in functions:
        violations[func.severity].append(func)

    print("RESUMO DE VIOLACOES MISRA:")
    print(f"  CRITICAL (N>=5):                   {len(violations[Severity.CRITICAL])}")
    print(f"  HIGH (>100 lines ou C>15):         {len(violations[Severity.HIGH])}")
    print(f"  MEDIUM (>50 lines, C>10, ou N=4):  {len(violations[Severity.MEDIUM])}")
    print(f"  LOW (>40 lines ou N=3):            {len(violations[Severity.LOW])}")
    print(f"  Sem violacoes:                     {len(violations[None])}")
    print()

    print("=" * 80)
    print("LISTA COMPLETA DE FUNCOES ATIVAS")
    print("=" * 80)
    print()

    for i, func in enumerate(functions, 1):
        status = "OK" if not func.violations else f"VIOLACAO {func.severity.value}"
        print(f"{i}. {func.name}")
        print(f"   Localizacao: Linhas {func.start_line}-{func.end_line}")
        print(f"   Metricas:    Lines={func.lines}, N={func.nesting}, C={func.complexity}")
        print(f"   Status:      {status}")
        if func.violations:
            print(f"   Violacoes:   {', '.join(func.violations)}")
        print()

    # Print prioritized violations
    if any(len(v) > 0 for k, v in violations.items() if k is not None):
        print("=" * 80)
        print("VIOLACOES MISRA PRIORIZADAS PARA REFATORACAO")
        print("=" * 80)
        print()

        for severity in [Severity.CRITICAL, Severity.HIGH, Severity.MEDIUM, Severity.LOW]:
            if violations[severity]:
                print(f"\n{'='*80}")
                print(f"{severity.value} - {len(violations[severity])} funcoes")
                print('='*80)

                for func in sorted(violations[severity], key=lambda f: (f.nesting, f.complexity, f.lines), reverse=True):
                    print(f"\n{func.name}")
                    print(f"  Localizacao: Linhas {func.start_line}-{func.end_line}")
                    print(f"  Metricas:    Lines={func.lines}, N={func.nesting}, C={func.complexity}")
                    print(f"  Violacoes:   {', '.join(func.violations)}")

    print()
    print("=" * 80)
    print("FIM DA ANALISE")
    print("=" * 80)

def main():
    file_path = r'C:\Users\AORUS-Desktop\Documents\88.Personal\1.Projects\scu-ecu-2.0-stm32f407-8x8\firmware\speeduino\speeduino\decoders.cpp'

    print("Analisando arquivo...")
    functions, stats = find_active_functions(file_path)

    print(f"Analise concluida! Encontradas {len(functions)} funcoes ativas.")
    print()

    print_report(functions, stats)

    # Save to file
    output_file = 'active_functions_report.txt'
    import sys
    with open(output_file, 'w', encoding='utf-8') as f:
        # Redirect stdout to file
        old_stdout = sys.stdout
        sys.stdout = f
        print_report(functions, stats)
        sys.stdout = old_stdout

    print(f"\nRelatorio salvo em: {output_file}")

if __name__ == '__main__':
    main()
