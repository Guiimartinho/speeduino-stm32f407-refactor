# =============================================================================
# SCG-ECU 2.0 - Custom Hooks Package
# =============================================================================
# Modularização e Adaptação Speeduino para STM32F407VGT6
# Projeto Base: Speeduino (https://speeduino.com) por Josh Stewart
#
# Este pacote contém hooks customizados para pre-commit:
#   - check_nesting.py: Verifica profundidade máxima de aninhamento (MISRA 15.6)
#   - check_secrets.py: Detecta credentials hardcoded
#   - check_ini_syntax.py: Valida sintaxe de arquivos INI TunerStudio
#
# =============================================================================

__version__ = "1.0.0"
__author__ = "SCG-ECU 2.0 Team"
