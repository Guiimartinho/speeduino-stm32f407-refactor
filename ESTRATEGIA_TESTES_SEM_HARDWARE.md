# ESTRATÉGIA DE TESTES SEM HARDWARE - ULTRATHINK

## SITUAÇÃO ATUAL

### Infraestrutura Existente

**Framework de Testes:** Unity (PlatformIO)
- 67 arquivos de teste
- 11 suítes de teste
- Ambiente native configurado

**Suítes de Teste Existentes:**
```
test_decoders       - Testes de decoders (trigger wheels)
test_fuel           - Testes de cálculos de combustível
test_ign            - Testes de ignição
test_init           - Testes de inicialização
test_math           - Testes matemáticos
test_schedule_calcs - Testes de cálculo de schedule
test_schedules      - Testes de schedulers
test_secondary      - Testes secundários
test_sensors        - Testes de sensores
test_table3d_native - Testes de tabelas 3D
test_tables         - Testes de tabelas
```

### Problema Atual

**STATUS:** Testes nativos não compilam
- Erro: `#include <Arduino.h>` não encontrado
- Causa: Ambiente native não tem stubs do Arduino
- Impacto: NENHUM teste pode ser executado sem hardware

---

## ESTRATÉGIA ULTRATHINK - 5 NÍVEIS

### NÍVEL 1: TESTES DE LÓGICA PURA (IMEDIATO - 1-2 dias)

**Objetivo:** Testar funções refatoradas que NÃO dependem de hardware

#### 1.1 Helper Functions de Cálculo

**FASE A - corrections.cpp:**
```cpp
// Testar helpers que só fazem cálculos
TEST(corrections, calculateDOT_MAP) {
  uint16_t MAP_change = 10;
  uint32_t deltaTime = 1000;

  currentStatus.mapDOT = calculateDOT_MAP(MAP_change, deltaTime);

  // MAP DOT = (1000000 / deltaTime) * MAP_change
  TEST_ASSERT_EQUAL(10000, currentStatus.mapDOT);
}

TEST(corrections, applyRPMTaper_no_taper) {
  uint16_t value = 100;
  currentStatus.RPM = 5000;
  configPage2.aeTaperMin = 1000;

  uint16_t result = applyRPMTaper(value);

  // RPM acima do taper min, sem redução
  TEST_ASSERT_EQUAL(100, result);
}

TEST(corrections, applyRPMTaper_full_taper) {
  uint16_t value = 100;
  currentStatus.RPM = 500;
  configPage2.aeTaperMin = 1000;
  configPage2.aeTaperMax = 2000;

  uint16_t result = applyRPMTaper(value);

  // RPM abaixo do taper max, redução aplicada
  // Expected: map(500, 1000, 2000, 0, 100) = 0
  TEST_ASSERT_EQUAL(0, result);
}
```

**FASE L-V - decoders.cpp (data-driven configs):**
```cpp
TEST(Nissan360, window_detection_4cyl_window1) {
  configPage4.nCylinders = 4;
  uint8_t duration = 16;

  bool found = processNissan360Window(duration, 4, &toothCurrentCount);

  TEST_ASSERT_TRUE(found);
  TEST_ASSERT_EQUAL(16, toothCurrentCount);
}

TEST(HondaJ32, tooth_angle_lookup_normal) {
  uint16_t toothCount = 5;

  int angle = getBaseAngle_HondaJ32(toothCount);

  // Tooth 5: 5 * 15 = 75 degrees
  TEST_ASSERT_EQUAL(75, angle);
}

TEST(HondaJ32, tooth_angle_lookup_unusual_14) {
  uint16_t toothCount = 14;

  int angle = getBaseAngle_HondaJ32(toothCount);

  // Tooth 14: 13*15 + 18 = 213 degrees (unusual spacing)
  TEST_ASSERT_EQUAL(213, angle);
}
```

**Ação:**
1. Criar `test/test_refactored_helpers/`
2. Copiar apenas as funções helper + structs
3. Mockar structs globais (currentStatus, configPage)
4. Executar testes isolados

**Vantagens:**
- Não precisa de Arduino.h
- Não precisa de hardware
- Validação imediata das refatorações
- Cobertura de 100% das funções helper

---

### NÍVEL 2: MOCKS DE HARDWARE (CURTO PRAZO - 3-5 dias)

**Objetivo:** Criar stubs mínimos do Arduino para testes nativos

#### 2.1 Arduino Mock Library

**Criar:** `test/mocks/Arduino.h`
```cpp
#ifndef ARDUINO_MOCK_H
#define ARDUINO_MOCK_H

#include <stdint.h>
#include <stdbool.h>

// Types
typedef uint8_t byte;
typedef bool boolean;

// Constants
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2

// Macros
#define PROGMEM
#define pgm_read_byte(addr) (*(const unsigned char *)(addr))
#define pgm_read_word(addr) (*(const unsigned short *)(addr))
#define pgm_read_dword(addr) (*(const unsigned long *)(addr))

// Global mock state
extern uint32_t mock_micros_value;
extern uint32_t mock_millis_value;
extern uint8_t mock_digital_pins[256];
extern uint16_t mock_analog_pins[64];

// Time functions
inline uint32_t micros(void) { return mock_micros_value; }
inline uint32_t millis(void) { return mock_millis_value; }
inline void delay(uint32_t ms) { mock_millis_value += ms; }
inline void delayMicroseconds(uint32_t us) { mock_micros_value += us; }

// GPIO functions
inline void pinMode(uint8_t pin, uint8_t mode) { /* no-op */ }
inline void digitalWrite(uint8_t pin, uint8_t val) { mock_digital_pins[pin] = val; }
inline int digitalRead(uint8_t pin) { return mock_digital_pins[pin]; }
inline void analogWrite(uint8_t pin, int val) { mock_analog_pins[pin] = val; }
inline int analogRead(uint8_t pin) { return mock_analog_pins[pin]; }

// Interrupts
inline void noInterrupts(void) { /* no-op */ }
inline void interrupts(void) { /* no-op */ }

// Math
inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

inline long constrain(long x, long a, long b) {
  if(x < a) return a;
  if(x > b) return b;
  return x;
}

#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#define abs(x) ((x)>0?(x):-(x))

// Serial mock (no-op)
class SerialMock {
public:
  void begin(unsigned long) { }
  void print(const char*) { }
  void println(const char*) { }
  int available() { return 0; }
  int read() { return -1; }
};
extern SerialMock Serial;

#endif // ARDUINO_MOCK_H
```

**Atualizar:** `platformio.ini`
```ini
[env:native]
platform = native
build_flags =
  -DUSE_LIBDIVIDE
  -std=c++14
  -Itest/mocks  ; Include mock Arduino.h
  -DNATIVE_BUILD  ; Flag to conditional compile
debug_build_flags = -std=c++14 -O0 -g3
; Remove test_ignore to run all tests
test_ignore =
debug_test = test_math
build_type = debug
```

**Ação:**
1. Criar `test/mocks/Arduino.h` com stubs
2. Criar `test/mocks/arduino_mock.cpp` com implementação
3. Atualizar platformio.ini para incluir mocks
4. Recompilar testes nativos

**Vantagens:**
- Permite compilar TODOS os testes
- Controle total sobre tempo (micros/millis)
- Controle total sobre GPIO (set/get)
- Isolamento completo

---

### NÍVEL 3: SIMULAÇÃO DE TRIGGERS (MÉDIO PRAZO - 1 semana)

**Objetivo:** Simular sinais de trigger wheel sem hardware

#### 3.1 Trigger Simulator

**Criar:** `test/simulators/trigger_simulator.h`
```cpp
class TriggerSimulator {
public:
  // Configurar padrão de trigger
  void configure(uint8_t teeth, uint8_t missing, uint16_t rpm) {
    _teeth = teeth;
    _missing = missing;
    _rpm = rpm;

    // Calcular período de cada dente
    _tooth_period_us = (60000000UL / rpm) / teeth;
  }

  // Simular próximo dente (primary)
  bool generatePrimaryTrigger() {
    _tooth_count++;

    // Dente faltante
    if (_tooth_count > (_teeth - _missing)) {
      _tooth_count = 1;
      return false; // Gap
    }

    // Avançar tempo
    mock_micros_value += _tooth_period_us;
    return true; // Dente detectado
  }

  // Simular trigger secundário (cam)
  bool generateSecondaryTrigger(uint8_t pattern[], uint8_t length) {
    if (_tooth_count == pattern[_sec_index % length]) {
      _sec_index++;
      return true;
    }
    return false;
  }

  void reset() {
    _tooth_count = 0;
    _sec_index = 0;
    mock_micros_value = 0;
  }

private:
  uint8_t _teeth;
  uint8_t _missing;
  uint16_t _rpm;
  uint32_t _tooth_period_us;
  uint8_t _tooth_count;
  uint8_t _sec_index;
};
```

**Exemplo de Teste:**
```cpp
TEST(decoders, Nissan360_sync_4cyl) {
  TriggerSimulator sim;
  sim.configure(360, 0, 3000); // 360 teeth, 0 missing, 3000 RPM

  uint8_t cam_pattern[] = {16, 102, 188, 274}; // 4-cyl windows

  // Simular até atingir sync
  for(uint16_t i = 0; i < 400; i++) {
    sim.generatePrimaryTrigger();

    if (sim.generateSecondaryTrigger(cam_pattern, 4)) {
      triggerSec_Nissan360(); // Chamar decoder
    }

    if (currentStatus.hasSync) {
      break; // Sync achieved
    }
  }

  TEST_ASSERT_TRUE(currentStatus.hasSync);
  TEST_ASSERT_EQUAL(4, toothCurrentCount); // Expected position
}
```

**Ação:**
1. Criar classe TriggerSimulator
2. Implementar padrões de triggers (36-1, 60-2, Nissan360, etc)
3. Criar testes para TODOS os decoders refatorados (L-V)
4. Validar sync detection, RPM calculation, angle calculation

**Vantagens:**
- Testar TODOS os decoders sem hardware
- Simular RPM variável
- Simular missing teeth
- Simular ruído (jitter)
- Validar todas as 18 refatorações de decoders

---

### NÍVEL 4: TESTES DE REGRESSÃO (MÉDIO PRAZO - 1 semana)

**Objetivo:** Garantir que refatorações não mudaram comportamento

#### 4.1 Snapshot Testing

**Conceito:** Capturar saída de função antes/depois da refatoração e comparar

**Criar:** `test/regression/snapshot_test.h`
```cpp
class SnapshotTest {
public:
  // Capturar estado antes da refatoração (do backup)
  void captureBaseline(const char* test_name) {
    // Executar código do backup
    // Salvar: toothCurrentCount, RPM, angle, etc
    _baseline[test_name] = getCurrentState();
  }

  // Executar código refatorado e comparar
  void verifyRefactored(const char* test_name) {
    State refactored = getCurrentState();
    State baseline = _baseline[test_name];

    TEST_ASSERT_EQUAL(baseline.toothCount, refactored.toothCount);
    TEST_ASSERT_EQUAL(baseline.rpm, refactored.rpm);
    TEST_ASSERT_EQUAL(baseline.angle, refactored.angle);
    // ... todas as variáveis de estado
  }

private:
  struct State {
    uint16_t toothCount;
    uint16_t rpm;
    int16_t angle;
    bool hasSync;
    // ... outros campos
  };

  std::map<const char*, State> _baseline;

  State getCurrentState() {
    return {
      toothCurrentCount,
      currentStatus.RPM,
      currentStatus.crankAngle,
      currentStatus.hasSync
    };
  }
};
```

**Exemplo de Teste:**
```cpp
TEST(regression, correctionAccel_MAP_mode) {
  SnapshotTest snap;

  // Setup inicial
  currentStatus.MAP = 100;
  currentStatus.TPS = 50;
  configPage2.aeMode = AE_MODE_MAP;

  // BASELINE: Código original (do backup)
  snap.captureBaseline("correctionAccel_MAP");

  // REFACTORED: Código novo
  uint16_t result = correctionAccel();
  snap.verifyRefactored("correctionAccel_MAP");

  // Bonus: verificar retorno
  TEST_ASSERT_EQUAL(100, result); // Expected no correction
}
```

**Ação:**
1. Para CADA fase refatorada (A, C, E-V)
2. Criar 10-20 casos de teste com inputs variados
3. Capturar outputs do código original (backups)
4. Comparar com outputs do código refatorado
5. Falha = lógica foi alterada

**Vantagens:**
- Prova matemática de equivalência
- Detecta qualquer mudança de comportamento
- Validação exaustiva
- Pode ser automatizado

---

### NÍVEL 5: COBERTURA E ANÁLISE (LONGO PRAZO - 2 semanas)

**Objetivo:** Medir qualidade dos testes e identificar gaps

#### 5.1 Code Coverage

**Ferramenta:** gcov + lcov (suportado pelo PlatformIO)

**Configurar:** `platformio.ini`
```ini
[env:native_coverage]
extends = env:native
build_flags =
  ${env:native.build_flags}
  -fprofile-arcs
  -ftest-coverage
  --coverage
build_unflags = -O2
test_build_src = yes
```

**Executar:**
```bash
# Rodar testes com coverage
pio test -e native_coverage

# Gerar relatório HTML
lcov --capture --directory .pio/build/native_coverage --output-file coverage.info
genhtml coverage.info --output-directory coverage_html

# Abrir coverage_html/index.html
```

**Meta:**
- Cobertura de linhas: >90%
- Cobertura de branches: >80%
- Cobertura de funções: 100%

#### 5.2 Mutation Testing

**Ferramenta:** MutPy ou similar

**Conceito:** Introduzir bugs intencionais e verificar se testes detectam

**Exemplo:**
```cpp
// Original
if (currentStatus.RPM > 1000) {
  return 100;
}

// Mutação 1: Trocar > por >=
if (currentStatus.RPM >= 1000) {
  return 100;
}

// Mutação 2: Trocar 1000 por 1001
if (currentStatus.RPM > 1001) {
  return 100;
}
```

Se os testes **NÃO detectarem** a mutação, significa que há gap de cobertura.

**Ação:**
1. Configurar mutation testing
2. Gerar mutações automaticamente
3. Rodar testes
4. Identificar mutações que sobrevivem
5. Adicionar testes para matar essas mutações

---

## ROADMAP DE IMPLEMENTAÇÃO

### SEMANA 1: FUNDAÇÃO
- [x] Análise da infraestrutura existente
- [ ] Criar Arduino mocks (`test/mocks/Arduino.h`)
- [ ] Fazer 1 teste simples compilar e passar
- [ ] Documentar setup para outros desenvolvedores

### SEMANA 2: TESTES DE HELPERS
- [ ] Criar `test/test_refactored_helpers/`
- [ ] Testes para FASE A helpers (6 functions)
- [ ] Testes para FASE C handlers (11 functions)
- [ ] Testes para data-driven configs (FASE L-V)
- [ ] Meta: 50+ testes, 100% cobertura de helpers

### SEMANA 3: TRIGGER SIMULATOR
- [ ] Implementar TriggerSimulator class
- [ ] Padrões de triggers: 36-1, 60-2, 24-1, Nissan360
- [ ] Testes para 5 decoders críticos
- [ ] Meta: Simular 1 revolução completa

### SEMANA 4: TESTES DE REGRESSÃO
- [ ] Implementar SnapshotTest
- [ ] 10 casos de teste para FASE A
- [ ] 10 casos de teste para FASE C
- [ ] 5 casos de teste para cada decoder (L-V)
- [ ] Meta: 100 testes de regressão

### SEMANA 5-6: COVERAGE E CI
- [ ] Configurar gcov/lcov
- [ ] Gerar relatórios de coverage
- [ ] Atingir 90% cobertura de linhas
- [ ] Integrar testes no CI/CD (GitHub Actions)

---

## BENEFÍCIOS ESPERADOS

### Curto Prazo (2 semanas)
- ✅ Validação automática de refatorações
- ✅ Detecção de bugs antes do hardware
- ✅ Confiança para continuar refatorando

### Médio Prazo (1 mês)
- ✅ 200+ testes automatizados
- ✅ Cobertura de 90%+
- ✅ CI/CD com testes automáticos
- ✅ Redução de 80% no tempo de validação

### Longo Prazo (3 meses)
- ✅ Plataforma de teste completa
- ✅ Simulação de todos os triggers
- ✅ Mutation testing
- ✅ Zero bugs em produção

---

## ESTIMATIVA DE ESFORÇO

| Tarefa | Tempo | Complexidade |
|--------|-------|--------------|
| Arduino Mocks | 2 dias | Baixa |
| Testes de Helpers | 3 dias | Baixa |
| Trigger Simulator | 5 dias | Média |
| Snapshot Testing | 5 dias | Média |
| Coverage Setup | 2 dias | Baixa |
| Testes de Regressão | 7 dias | Alta |
| CI/CD Integration | 3 dias | Média |
| Documentação | 3 dias | Baixa |
| **TOTAL** | **30 dias** | **~1 mês** |

---

## PRIORIZAÇÃO ULTRATHINK

### CRÍTICO (FAZER AGORA)
1. Arduino Mocks - sem isso, nenhum teste compila
2. Testes de Helpers - validar refatorações já feitas

### IMPORTANTE (PRÓXIMOS 2 SEMANAS)
3. Trigger Simulator - maior valor para ECU
4. Snapshot Testing - prova matemática de equivalência

### DESEJÁVEL (PRÓXIMO MÊS)
5. Coverage Analysis - encontrar gaps
6. CI/CD Integration - automatizar

---

## COMANDOS ÚTEIS

```bash
# Listar testes disponíveis
pio test -e native --list-tests

# Rodar teste específico
pio test -e native -f test_math

# Rodar com verbosidade
pio test -e native -v

# Rodar com coverage
pio test -e native_coverage

# Gerar relatório de coverage
lcov --capture --directory .pio/build/native_coverage --output-file coverage.info
genhtml coverage.info --output-directory coverage_html
```

---

## PRÓXIMOS PASSOS IMEDIATOS

**AÇÃO 1:** Criar Arduino mocks
```bash
mkdir -p test/mocks
# Criar test/mocks/Arduino.h (usar template acima)
# Criar test/mocks/arduino_mock.cpp
```

**AÇÃO 2:** Atualizar platformio.ini
```ini
[env:native]
build_flags =
  -DUSE_LIBDIVIDE
  -std=c++14
  -Itest/mocks
  -DNATIVE_BUILD
test_ignore =  ; Remover para habilitar todos os testes
```

**AÇÃO 3:** Tentar compilar 1 teste
```bash
pio test -e native -f test_math -v
```

**AÇÃO 4:** Criar primeiro teste de helper
```bash
mkdir -p test/test_refactored_helpers
# Criar test/test_refactored_helpers/test_corrections_helpers.cpp
```

---

**Data:** 2025-10-31
**Metodologia:** ULTRATHINK
**Status:** PROPOSTA COMPLETA
