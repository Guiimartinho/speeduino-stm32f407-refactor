# Plano de Cobertura de Testes Completo - SCG-ECU 2.0

**Data:** 06/11/2025
**Versão:** 1.0
**Status:** 📋 PLANEJAMENTO - Pronto para Execução

---

## 🎯 OBJETIVO

Atingir **95%+ de cobertura de testes de software** para o projeto SCG-ECU 2.0, garantindo qualidade, confiabilidade e facilitando manutenção futura.

---

## 📊 SITUAÇÃO ATUAL

### Status de Testes

| Métrica | Atual | Meta | Gap |
|---------|-------|------|-----|
| **Total Tests** | 313 | 700+ | +387 tests |
| **Módulos Testados** | 16/42 (38%) | 41/42 (98%) | +25 módulos |
| **Test Suites** | 18 | 44+ | +26 suites |
| **Cobertura Estimada** | ~40% | ~95% | +55% |
| **Test Execution Time** | ~4.5s | ~15s | +10.5s |

### CI/CD Status

✅ **GitHub Actions - 4 Workflows Configurados:**

1. **ci.yml** - Build + Tests + Memory Report
   - Build STM32F407 firmware
   - Executar 313 unit tests (7 suites)
   - Memory delta report (PRs)
   - Code quality checks (codespell)
   - INI validation

2. **misra.yml** - MISRA C:2012 Compliance
   - cppcheck scan
   - Violation tracking
   - PR comments com resultados
   - Threshold: <1000 violations

3. **doxygen.yml** - Documentation Build
   - (não verificado ainda)

4. **release.yml** - Release Management
   - (não verificado ainda)

**Status CI/CD:** ✅ **EXCELENTE** - Pipeline completo implementado

---

## 📋 MÓDULOS POR PRIORIDADE

### 🔴 PRIORIDADE ALTA - Core Modules (10 módulos)

Módulos críticos, já refatorados, necessitam testes completos:

1. **init.cpp** (2721 linhas, 84 funções, 21+ helpers)
   - Test suite: `test_init_full/`
   - Testes: ~60 tests
   - Tempo: 3 dias

2. **speeduino.cpp** (main loop)
   - Test suite: `test_speeduino/`
   - Testes: ~40 tests
   - Tempo: 2 dias

3. **timers.cpp** (ISR context - CRITICAL)
   - Test suite: `test_timers/`
   - Testes: ~30 tests
   - Tempo: 2 dias

4. **updates.cpp** (EEPROM migrations)
   - Test suite: `test_updates/`
   - Testes: ~25 tests
   - Tempo: 2 dias

5. **auxiliaries.cpp** (6 sub-módulos)
   - Test suite: `test_auxiliaries/`
   - Testes: ~50 tests (fan, boost, VVT, AC, nitrous, WMI)
   - Tempo: 3 dias

6. **utilities.cpp**
   - Test suite: `test_utilities/`
   - Testes: ~30 tests
   - Tempo: 1 dia

7. **logger.cpp**
   - Test suite: `test_logger/`
   - Testes: ~20 tests
   - Tempo: 1 dia

8. **storage.cpp** (EEPROM + CRC)
   - Test suite: `test_storage/`
   - Testes: ~35 tests
   - Tempo: 2 dias

9. **fuel_calculations.cpp** (completar cobertura)
   - Test suite: `test_fuel_calculations_complete/`
   - Testes: ~25 tests (adicionar aos ~15 existentes)
   - Tempo: 1 dia

10. **communication_handler.cpp** (novo módulo)
    - Test suite: `test_communication_handler/`
    - Testes: ~20 tests
    - Tempo: 1 dia

**Subtotal:** ~335 novos tests | **Tempo:** 18 dias úteis

---

### 🟡 PRIORIDADE MÉDIA - Comms & Peripherals (8 módulos)

Módulos de comunicação e periféricos:

11. **comms.cpp**
    - Test suite: `test_comms/`
    - Testes: ~40 tests
    - Tempo: 2 dias

12. **comms_legacy.cpp**
    - Test suite: `test_comms_legacy/`
    - Testes: ~15 tests
    - Tempo: 1 dia

13. **comms_secondary.cpp**
    - Test suite: `test_comms_secondary/`
    - Testes: ~15 tests
    - Tempo: 1 dia

14. **comms_CAN.cpp**
    - Test suite: `test_comms_can/`
    - Testes: ~30 tests
    - Tempo: 2 dias

15. **pages.cpp** (config pages)
    - Test suite: `test_pages/`
    - Testes: ~25 tests
    - Tempo: 2 dias

16. **page_crc.cpp**
    - Test suite: `test_page_crc/`
    - Testes: ~10 tests
    - Tempo: 0.5 dia

17. **SD_logger.cpp**
    - Test suite: `test_sd_logger/`
    - Testes: ~20 tests (com SD mock)
    - Tempo: 1.5 dias

18. **TS_CommandButtonHandler.cpp**
    - Test suite: `test_ts_commands/`
    - Testes: ~15 tests
    - Tempo: 1 dia

**Subtotal:** ~170 novos tests | **Tempo:** 11 dias úteis

---

### 🟢 PRIORIDADE BAIXA - Hardware & Drivers (7 módulos + LED system)

Drivers e hardware específico:

19. **led_button_system.cpp** ⭐ NOVO (890 linhas, 28 funções)
    - Test suite: `test_led_button_system/`
    - Testes: ~50 tests
      - Button debouncing (5 tests)
      - Press type detection - 5 tipos (10 tests)
      - LED pattern generation (8 tests)
      - Mode transitions - 5 modos (10 tests)
      - EEPROM persistence (7 tests)
      - Error detection (7 sensors, 7 tests)
      - Startup sequence (3 tests)
    - Tempo: 2 dias

20. **board_stm32_official.cpp**
    - Test suite: `test_board_stm32/`
    - Testes: ~15 tests
    - Tempo: 1 dia

21. **acc_mc33810.cpp**
    - Test suite: `test_acc_mc33810/`
    - Testes: ~12 tests
    - Tempo: 1 dia

22. **rtc_common.cpp**
    - Test suite: `test_rtc/`
    - Testes: ~10 tests
    - Tempo: 0.5 dia

23. **sensor_polling.cpp**
    - Test suite: `test_sensor_polling/`
    - Testes: ~20 tests
    - Tempo: 1 dia

24. **scheduledIO.cpp**
    - Test suite: `test_scheduled_io/`
    - Testes: ~18 tests
    - Tempo: 1 dia

25. **table3d_interpolate.cpp**
    - Test suite: `test_table3d_interpolate/`
    - Testes: ~15 tests
    - Tempo: 1 dia

26. **globals.cpp**
    - **SKIP** - Apenas definições globais, não requer testes

**Subtotal:** ~140 novos tests | **Tempo:** 7.5 dias úteis

---

## 📈 RESUMO QUANTITATIVO

### Testes por Fase

| Fase | Módulos | Novos Tests | Tempo (dias) | Acumulado Tests |
|------|---------|-------------|--------------|-----------------|
| **Atual** | 16 | 313 | - | **313** |
| **FASE 1 (ALTA)** | +10 | +335 | 18 | **648** |
| **FASE 2 (MÉDIA)** | +8 | +170 | 11 | **818** |
| **FASE 3 (BAIXA)** | +7 | +140 | 7.5 | **958** |
| **TOTAL** | **41** | **+645** | **36.5** | **958** |

### Cronograma Consolidado

**Início:** 07/11/2025 (quinta-feira)
**Fim Estimado:** 27/12/2025 (sexta-feira)
**Duração Total:** ~7 semanas (36.5 dias úteis)

### Esforço Necessário

- **1 desenvolvedor full-time:** ~7 semanas
- **2 desenvolvedores:** ~4 semanas
- **Sprint approach (50% time):** ~14 semanas

---

## 🛠️ TEMPLATE DE IMPLEMENTAÇÃO

### Estrutura de Diretório

```
test/test_<module_name>/
├── test_<module_name>.cpp          # Main test file
├── test_<specific_function>.cpp    # Function-specific tests
├── README.md                       # Test documentation
└── mocks/                          # Module-specific mocks (se necessário)
    └── mock_<dependency>.h
```

### Template de Código

```cpp
// test/test_<module>/test_<module>.cpp
#include <unity.h>
#include "../../speeduino/<module>.h"
#include "Arduino.h"  // Arduino mocks

// ============================================================================
// SETUP & TEARDOWN
// ============================================================================

void setUp(void) {
    // Reset state before each test
    // Initialize mocks
}

void tearDown(void) {
    // Cleanup after each test
}

// ============================================================================
// TEST CASES
// ============================================================================

void test_<function>_<scenario>_<expected_result>(void) {
    // ARRANGE: Setup inputs and expected outputs
    uint16_t input = 100;
    uint16_t expected = 200;

    // ACT: Execute function under test
    uint16_t actual = <function>(input);

    // ASSERT: Verify result
    TEST_ASSERT_EQUAL_UINT16(expected, actual);
}

void test_<function>_edge_case_boundary_values(void) {
    // Test boundary conditions
    TEST_ASSERT_EQUAL(0, <function>(0));
    TEST_ASSERT_EQUAL(255, <function>(255));
}

void test_<function>_error_handling_invalid_input(void) {
    // Test error conditions
    TEST_ASSERT_EQUAL(-1, <function>(INVALID_VALUE));
}

// ============================================================================
// TEST RUNNER
// ============================================================================

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Register tests
    RUN_TEST(test_<function>_<scenario>_<expected_result>);
    RUN_TEST(test_<function>_edge_case_boundary_values);
    RUN_TEST(test_<function>_error_handling_invalid_input);

    return UNITY_END();
}
```

### Naming Convention

```
test_<function_name>_<scenario>_<expected_result>

Exemplos:
- test_calculateInjectorPulseWidth_normalOperation_returnsCorrectValue
- test_buttonDebounce_rapidPresses_ignoresNoise
- test_ledSetMode_invalidMode_clipsToValid
- test_eepromWrite_wearLeveling_avoidsRepeatedWrites
```

---

## 🎯 METAS DE QUALIDADE

### Code Coverage

| Categoria | Meta | Estratégia |
|-----------|------|------------|
| **Line Coverage** | >90% | Statement coverage |
| **Branch Coverage** | >85% | Decision coverage |
| **Function Coverage** | >95% | All public APIs |
| **Helper Coverage** | 100% | All 187 extracted helpers |

### Test Quality

- **Pass Rate:** 100% (zero tolerance para falhas)
- **Execution Time:** <20 segundos (todas as 958 tests)
- **Isolation:** Zero dependências entre testes
- **Determinism:** 100% reproduzível
- **Documentation:** 100% dos testes documentados

### CI/CD Integration

- ✅ **Todos os testes executados automaticamente** no CI/CD
- ✅ **Build bloqueado** se tests falham
- ✅ **Coverage report** gerado e rastreado
- ✅ **PR checks** obrigatórios antes de merge

---

## 🚀 PRÓXIMOS PASSOS IMEDIATOS

### AÇÃO 1: Executar Baseline de Testes Atuais

```bash
# Executar todos os 313 tests atuais
platformio test -e native -v \
  -f test_refactored_helpers \
  -f test_corrections_massive \
  -f test_decoders_massive \
  -f test_sensors_massive \
  -f test_idle_massive \
  -f test_engineProtection_massive \
  -f test_scheduling_massive
```

**Resultado Esperado:** 313/313 PASSED (100%)

---

### AÇÃO 2: Criar Primeiro Test Suite - test_led_button_system/

**Justificativa:**
- Módulo recém-implementado (06/11/2025)
- 890 linhas, 28 funções
- Ainda não rastreado no git
- Sistema completo e isolado (ideal para testes)

**Comandos:**

```bash
# Criar estrutura
mkdir -p test/test_led_button_system

# Criar main test file
cat > test/test_led_button_system/test_led_button_system.cpp << 'EOF'
#include <unity.h>
#include "../../speeduino/led_button_system.h"
#include "Arduino.h"

void setUp(void) {
    ledButtonInit();
}

void tearDown(void) {
    // Cleanup
}

// Button Debouncing Tests (5 tests)
void test_button_debounce_shortPress_detectedCorrectly(void) {
    // TODO: Implement
    TEST_IGNORE_MESSAGE("To be implemented");
}

// TODO: Add remaining 49 tests

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_button_debounce_shortPress_detectedCorrectly);
    return UNITY_END();
}
EOF

# Executar test
platformio test -e native -f test_led_button_system
```

---

### AÇÃO 3: Atualizar CI/CD com Nova Test Suite

Editar `.github/workflows/ci.yml`:

```yaml
# Line 130-137: Adicionar test_led_button_system
platformio test -e native -v \
  -f test_refactored_helpers \
  -f test_corrections_massive \
  -f test_decoders_massive \
  -f test_sensors_massive \
  -f test_idle_massive \
  -f test_engineProtection_massive \
  -f test_scheduling_massive \
  -f test_led_button_system  # NOVO
```

Atualizar contadores:

```yaml
# Line 92-93
unit-tests:
  name: Unit Tests (363 tests)  # 313 + 50 LED

# Line 120-128
echo "=== Running 363 Unit Tests (8 test suites) ==="
echo "  - test_led_button_system (50 tests)"  # ADICIONAR
```

---

### AÇÃO 4: Documentar Progress

Criar relatório:

```bash
# Criar relatório de progresso
cat > docs/reports/18_RELATORIO_COBERTURA_TESTES_FASE1.md

# Conteúdo:
# - Baseline: 313 tests, 16 módulos (38%)
# - Meta: 958 tests, 41 módulos (98%)
# - Fase 1 iniciada: test_led_button_system (50 tests)
# - Cronograma: 7 semanas (07/11 - 27/12/2025)
```

---

## 📊 KPIs E TRACKING

### Métricas Semanais

| Semana | Data | Tests | Módulos | Cobertura | Status |
|--------|------|-------|---------|-----------|--------|
| 0 (Baseline) | 06/11/2025 | 313 | 16 | 38% | ✅ Done |
| 1 | 13/11/2025 | 423 | 19 | 45% | 📋 Plan |
| 2 | 20/11/2025 | 533 | 22 | 52% | 📋 Plan |
| 3 | 27/11/2025 | 643 | 25 | 60% | 📋 Plan |
| 4 | 04/12/2025 | 728 | 29 | 70% | 📋 Plan |
| 5 | 11/12/2025 | 813 | 33 | 80% | 📋 Plan |
| 6 | 18/12/2025 | 888 | 37 | 88% | 📋 Plan |
| 7 | 27/12/2025 | 958 | 41 | 95%+ | 🎯 Goal |

### Relatórios Requeridos

Criar ao final de cada fase:

- **18_RELATORIO_COBERTURA_TESTES_FASE1.md** (ALTA)
- **19_RELATORIO_COBERTURA_TESTES_FASE2.md** (MÉDIA)
- **20_RELATORIO_COBERTURA_TESTES_FASE3.md** (BAIXA)
- **21_RELATORIO_COBERTURA_TESTES_FINAL.md** (COMPLETO)

---

## ✅ CHECKLIST DE VALIDAÇÃO

### Para Cada Novo Test Suite

- [ ] Diretório criado: `test/test_<module>/`
- [ ] Main test file: `test_<module>.cpp`
- [ ] README.md com documentação
- [ ] Testes compilam sem warnings
- [ ] Todos os tests PASS (100%)
- [ ] Mocks adequados implementados
- [ ] CI/CD atualizado para executar suite
- [ ] Documentação atualizada
- [ ] Relatório de fase atualizado

### Para Completar o Projeto

- [ ] 958+ tests implementados
- [ ] 41/42 módulos cobertos (98%)
- [ ] 100% pass rate
- [ ] <20s execution time
- [ ] >90% line coverage
- [ ] >85% branch coverage
- [ ] CI/CD verde (todas as 4 workflows)
- [ ] Documentação completa (relatórios 18-21)
- [ ] Aprovação do usuário

---

## 🎓 REFERÊNCIAS

### Documentos Relacionados

- **ANALISE_COBERTURA_TESTES.md** - Análise detalhada dos gaps
- **docs/reference/07_ESTRATEGIA_TESTES_SEM_HARDWARE.md** - Estratégia 5 níveis
- **docs/reports/12_RELATORIO_FASE_V_VALIDATION_TESTING.md** - Fase V infrastructure
- **docs/reports/13_RELATORIO_FASE_V_COMPLETO.md** - 313 tests baseline

### Ferramentas

- **Unity** - Framework de testes (http://www.throwtheswitch.org/unity)
- **PlatformIO** - Build system e test runner
- **GitHub Actions** - CI/CD pipeline
- **cppcheck** - MISRA compliance
- **gcov/lcov** - Coverage analysis (Fase futura)

---

**Versão:** 1.0
**Última Atualização:** 06/11/2025
**Status:** 📋 **PLANEJAMENTO COMPLETO** - Pronto para Execução
**Aprovação Requerida:** Sim
**Próximo Passo:** Executar AÇÃO 1 (baseline tests) + AÇÃO 2 (LED system tests)

---

🚀 **SCG-ECU 2.0 - Cobertura de Testes Completa - 7 Semanas para 95%+**
