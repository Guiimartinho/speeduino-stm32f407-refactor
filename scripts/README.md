# Scripts de Qualidade de Código - SCG-ECU 2.0
## Modularização e Adaptação Speeduino para STM32F407VGT6

**Projeto Base:** [Speeduino](https://speeduino.com) por Josh Stewart
**Data:** 2025-12-30
**Status:** ✅ IMPLEMENTADO

---

## 📋 Visão Geral

Este diretório contém scripts de qualidade de código para o projeto SCG-ECU 2.0, incluindo:

- **Pre-commit hooks** para validação automática
- **Análise estática** com cppcheck e MISRA
- **Execução de testes** unitários
- **Verificação de build** e memória

---

## 🚀 Instalação Rápida

```bash
# 1. Instalar pre-commit
pip install pre-commit

# 2. Instalar hooks
cd firmware/speeduino
pre-commit install
pre-commit install --hook-type pre-push

# 3. Verificar instalação
pre-commit run --all-files
```

---

## 📁 Estrutura

```
scripts/
├── README.md                 # Esta documentação
├── __init__.py              # Pacote Python
├── run_cppcheck.py          # Análise estática cppcheck
├── run_tests.py             # Executor de testes
├── run_build.py             # Verificação de build
└── hooks/
    ├── __init__.py          # Pacote hooks
    ├── check_nesting.py     # MISRA 15.6 - máx 3 níveis
    ├── check_secrets.py     # Detecção de credentials
    └── check_ini_syntax.py  # Validação INI TunerStudio
```

---

## 🔗 Pre-commit Hooks

### Hooks Padrão (Higiene)

| Hook | Descrição |
|------|-----------|
| `trailing-whitespace` | Remove espaços em branco no final das linhas |
| `end-of-file-fixer` | Garante newline no final dos arquivos |
| `check-yaml` | Valida sintaxe YAML |
| `check-json` | Valida sintaxe JSON |
| `check-xml` | Valida sintaxe XML |
| `check-added-large-files` | Bloqueia arquivos > 500KB |
| `mixed-line-ending` | Força LF (Unix line endings) |
| `check-merge-conflict` | Detecta marcadores de conflito |
| `check-case-conflict` | Detecta conflitos de case |

### Hooks Customizados

| Hook | Arquivo | Descrição |
|------|---------|-----------|
| `check-nesting` | `check_nesting.py` | Profundidade máx. 3 (MISRA 15.6) |
| `check-secrets` | `check_secrets.py` | Senhas/API keys hardcoded |
| `check-cppcheck-quick` | `run_cppcheck.py` | Análise rápida (erros críticos) |
| `check-ini-syntax` | `check_ini_syntax.py` | Sintaxe INI TunerStudio |

### Pre-push Hooks

| Hook | Arquivo | Descrição |
|------|---------|-----------|
| `unit-tests` | `run_tests.py` | Testes unitários antes do push |
| `build-check` | `run_build.py` | Verifica se compila |

---

## 🔍 Scripts de Análise

### run_cppcheck.py

Executa cppcheck com configurações otimizadas para o projeto.

```bash
# Análise completa
python scripts/run_cppcheck.py

# Análise rápida (só erros)
python scripts/run_cppcheck.py --quick

# Com verificação MISRA
python scripts/run_cppcheck.py --misra

# Verbose
python scripts/run_cppcheck.py -v
```

**Configurações:**
- Enable: warning, performance, portability, style
- Supressões: falsos positivos conhecidos (libdivide, callbacks)
- C++14, STM32F407xx defines

### run_tests.py

Executa testes unitários via PlatformIO.

```bash
# Todos os testes
python scripts/run_tests.py

# Testes rápidos (core apenas)
python scripts/run_tests.py --quick

# Suite específica
python scripts/run_tests.py --suite test_corrections_massive

# Listar suites disponíveis
python scripts/run_tests.py --list

# Verbose
python scripts/run_tests.py -v
```

**Suites Core (--quick):**
- `test_refactored_helpers`
- `test_corrections_massive`

### run_build.py

Verifica se o firmware compila e está dentro do budget de memória.

```bash
# Build normal
python scripts/run_build.py

# Verificação rápida (para hooks)
python scripts/run_build.py --check

# Clean build
python scripts/run_build.py --clean

# Environment específico
python scripts/run_build.py --env black_F407VE-EEPROM-SPI
```

**Budgets:**
- Flash: ≤ 45% (235KB de 512KB)
- RAM: ≤ 20% (26KB de 128KB)

---

## 🔐 Detecção de Secrets

O `check_secrets.py` detecta:

```
├── password = "..."
├── api_key = "..."
├── secret = "..."
├── token = "..." (>20 chars)
├── -----BEGIN PRIVATE KEY-----
├── aws_access_key_id = "AKIA..."
├── aws_secret_access_key = "..."
├── github_pat_*
└── Database URLs com credentials
```

**Supressão inline:**
```c
const char* test_password = "test123";  // SECRET_OK
```

---

## 📏 Verificação de Nesting

O `check_nesting.py` verifica profundidade máxima de 3 níveis (MISRA C:2012 Rule 15.6).

**Exemplo de violação:**
```c
void func() {
    if (a) {                    // Nível 1
        if (b) {                // Nível 2
            if (c) {            // Nível 3
                if (d) {        // Nível 4 - VIOLAÇÃO!
                    // ...
                }
            }
        }
    }
}
```

**Supressão inline:**
```c
if (condition) {  // NESTING_OK
    // Código com nesting profundo justificado
}
```

---

## 🛠️ Requisitos

### Python
```bash
pip install pre-commit
```

### Ferramentas Externas

| Ferramenta | Instalação |
|------------|------------|
| **cppcheck** | `winget install cppcheck` (Windows) |
| | `sudo apt install cppcheck` (Linux) |
| | `brew install cppcheck` (macOS) |
| **PlatformIO** | `pip install platformio` |

---

## ⚙️ Configuração Avançada

### Ignorar Diretórios

Edite `.pre-commit-config.yaml`:
```yaml
- id: check-nesting
  exclude: ^(libdivide/|test/mocks/|docs/)
```

### Alterar Limite de Nesting

Edite `check_nesting.py`:
```python
DEFAULT_MAX_DEPTH = 4  # Aumentar para 4 se necessário
```

### Adicionar Palavras ao Codespell

Edite `.github/workflows/codespell-ignored-words.txt`:
```
palavra_a_ignorar
outra_palavra
```

---

## 🔄 Workflow de Desenvolvimento

```
┌─────────────────────────────────────────────────────────────────┐
│                         git commit                               │
└─────────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│                     PRE-COMMIT HOOKS                             │
├─────────────────────────────────────────────────────────────────┤
│  ✓ trailing-whitespace    ✓ check-yaml    ✓ check-nesting       │
│  ✓ end-of-file-fixer      ✓ check-json    ✓ check-secrets       │
│  ✓ mixed-line-ending      ✓ codespell     ✓ cppcheck-quick      │
└─────────────────────────────────────────────────────────────────┘
                              ↓
                         COMMIT OK ✓
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│                          git push                                │
└─────────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│                      PRE-PUSH HOOKS                              │
├─────────────────────────────────────────────────────────────────┤
│  ✓ unit-tests (quick)     ✓ build-check                         │
└─────────────────────────────────────────────────────────────────┘
                              ↓
                          PUSH OK ✓
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│                      GITHUB ACTIONS CI                           │
├─────────────────────────────────────────────────────────────────┤
│  ✓ Build STM32    ✓ Unit Tests    ✓ MISRA Scan    ✓ Memory     │
└─────────────────────────────────────────────────────────────────┘
```

---

## 📊 Comparação com CI

| Verificação | Local (Pre-commit) | CI (GitHub Actions) |
|-------------|-------------------|---------------------|
| Whitespace/EOL | ✓ Automático | - |
| YAML/JSON syntax | ✓ Automático | - |
| Codespell | ✓ Automático | ✓ |
| Nesting depth | ✓ Automático | - |
| Secrets | ✓ Automático | - |
| cppcheck | ✓ Quick | ✓ Full |
| MISRA | ✓ Opcional | ✓ Completo |
| Unit tests | ✓ Quick (pre-push) | ✓ Full (648+ tests) |
| Build | ✓ Check (pre-push) | ✓ Full |
| Memory report | - | ✓ (PRs only) |

---

## 🐛 Troubleshooting

### Pre-commit não executa

```bash
# Reinstalar hooks
pre-commit uninstall
pre-commit install
pre-commit install --hook-type pre-push
```

### cppcheck não encontrado

```bash
# Verificar instalação
cppcheck --version

# Adicionar ao PATH (Windows)
set PATH=%PATH%;C:\Program Files\Cppcheck
```

### Testes falham localmente

```bash
# Verificar ambiente
pio test -e native -v

# Limpar cache
pio run -t clean
```

### Hook muito lento

```bash
# Pular hook específico temporariamente
SKIP=check-cppcheck-quick git commit -m "mensagem"
```

---

## 📚 Referências

- [Pre-commit Documentation](https://pre-commit.com/)
- [Cppcheck Manual](http://cppcheck.net/manual.pdf)
- [PlatformIO Testing](https://docs.platformio.org/en/latest/advanced/unit-testing/index.html)
- [MISRA C:2012](https://www.misra.org.uk/)

---

**Projeto:** SCG-ECU 2.0 STM32F407VGT6
**Status:** ✅ INFRAESTRUTURA COMPLETA
