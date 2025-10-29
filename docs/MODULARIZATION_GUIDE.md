# Guia de Referência: Modularização do Firmware Speeduino STM32F407

## 📋 Índice
1. [Visão Geral](#visão-geral)
2. [Princípios Fundamentais](#princípios-fundamentais)
3. [Etapas de Modularização](#etapas-de-modularização)
4. [Padrões Obrigatórios](#padrões-obrigatórios)
5. [Checklist de Qualidade](#checklist-de-qualidade)
6. [Fase 1: auxiliaries.cpp](#fase-1-auxiliariescpp)
7. [Fase 2: corrections.cpp](#fase-2-correctionscpp)
8. [Fase 3: init.cpp](#fase-3-initcpp)
9. [Exemplos de Código](#exemplos-de-código)
10. [Troubleshooting](#troubleshooting)

---

## 🎯 Visão Geral

### Objetivo
Modularizar 3 arquivos monolíticos (6.298 linhas totais) em módulos especializados seguindo padrões profissionais de código automotivo.

### Resultado Esperado
- ✅ Arquivos com **máximo 300 linhas**
- ✅ **Zero arquivos >1000 linhas**
- ✅ **Max 2-3 níveis de nesting**
- ✅ **Conformidade MISRA C++**
- ✅ **Cobertura de testes 60%+**
- ✅ **Build time -22%**

### Ordem de Execução

| Fase | Arquivo | Linhas | Horas | Risco | Benefício |
|------|---------|--------|-------|-------|-----------|
| **FASE 1** | auxiliaries.cpp | 1.283 | 12-16h | 5/10 | 9/10 |
| **FASE 2** | corrections.cpp | 1.123 | 10-14h | 4/10 | 8/10 |
| **FASE 3** | init.cpp | 3.892 | 24-32h | 7/10 | 10/10 |

---

## 🏗️ Princípios Fundamentais

### 1. No-Nesting (Máximo 2-3 níveis)

**❌ EVITAR:**
```cpp
if (condition1) {
    if (condition2) {
        if (condition3) {
            if (condition4) {
                // Código aqui (4 níveis - RUIM!)
            }
        }
    }
}
```

**✅ USAR Guard Clauses:**
```cpp
void function() {
    // Validações primeiro (early return)
    if (!condition1) { return; }
    if (!condition2) { return; }
    if (!condition3) { return; }

    // Código principal sem nesting
    doSomething();
}
```

**✅ USAR State Machine:**
```cpp
enum class State : uint8_t {
    IDLE,
    ACTIVE,
    ERROR
};

void update() {
    switch (currentState) {
        case State::IDLE:
            handleIdleState();
            break;
        case State::ACTIVE:
            handleActiveState();
            break;
        case State::ERROR:
            handleErrorState();
            break;
    }
}
```

### 2. Single Responsibility Principle (SRP)

**Cada módulo/função tem UMA responsabilidade:**

❌ **ERRADO** - Função faz múltiplas coisas:
```cpp
void updateBoost() {
    // Lê sensor
    readMAP();

    // Calcula target
    calculateTarget();

    // Roda PID
    runPID();

    // Aplica saída
    applyPWM();

    // Loga dados
    logToSD();
}
```

✅ **CORRETO** - Cada função uma responsabilidade:
```cpp
void updateBoost() {
    uint16_t map = readMAP();
    uint16_t target = calculateTarget();
    uint16_t duty = runPID(target, map);
    applyPWM(duty);
}

void logBoostData() {
    // Separado - responsabilidade diferente
    logToSD(currentStatus.boostDuty);
}
```

### 3. MISRA C++ Compliance (Automotive Standards)

**Regras Críticas:**

**Rule 8.2 - Protótipos com nomes:**
```cpp
// ❌ ERRADO
void func(int, int);

// ✅ CORRETO
void calculatePulsewidth(uint16_t reqFuel, uint16_t corrections);
```

**Rule 8.7 - Funções static quando possível:**
```cpp
// ❌ ERRADO - linkage desnecessário
void helperFunction() { }

// ✅ CORRETO - escopo local
static void helperFunction() { }
```

**Rule 10.3 - Type casting explícito:**
```cpp
// ❌ ERRADO - conversão implícita
uint16_t a = 100;
uint32_t b = a * 256; // Pode overflow

// ✅ CORRETO - type promotion explícito
uint16_t a = 100U;
uint32_t b = (uint32_t)a * 256U;
```

**Rule 14.4 - Expressões booleanas:**
```cpp
// ❌ ERRADO
if (value) { }

// ✅ CORRETO
if (value != 0U) { }
if (ptr != nullptr) { }
```

**Rule 15.5 - Single exit point:**
```cpp
// ❌ EVITAR - múltiplos returns
bool validate() {
    if (error1) return false;
    if (error2) return false;
    if (error3) return false;
    return true;
}

// ✅ PREFERIR - single exit
bool validate() {
    bool result = true;

    if (error1) {
        result = false;
    } else if (error2) {
        result = false;
    } else if (error3) {
        result = false;
    }

    return result; // Único ponto de saída
}

// ✅ ACEITÁVEL para guard clauses em funções void
void process() {
    if (!isValid) { return; } // Early return OK
    if (!isReady) { return; } // Guard clause OK

    // Processamento principal
}
```

### 4. Automotive Safety

**Watchdog Protection:**
```cpp
void criticalFunction() {
    watchdog_refresh(); // Reset watchdog

    // Operação crítica

    watchdog_refresh(); // Reset após operação
}
```

**Bounds Checking:**
```cpp
void setInjectorChannel(uint8_t channel, uint16_t pulsewidth) {
    // Validação de entrada
    if (channel < 1U || channel > 8U) {
        logError("Invalid injector channel: ", channel);
        return;
    }

    if (pulsewidth > MAX_PULSEWIDTH) {
        logWarning("Pulsewidth clamped to max");
        pulsewidth = MAX_PULSEWIDTH;
    }

    // Acesso seguro
    injectorPW[channel - 1U] = pulsewidth;
}
```

**Overflow Protection:**
```cpp
uint32_t multiplyWithOverflowCheck(uint16_t a, uint16_t b) {
    const uint32_t MAX_RESULT = 100000U;

    // Verificar overflow antes de multiplicar
    if (a > (MAX_RESULT / b)) {
        logError("Multiplication would overflow!");
        return MAX_RESULT;
    }

    return (uint32_t)a * (uint32_t)b;
}
```

**Timeout Protection:**
```cpp
bool waitForSensor(uint32_t timeout_ms) {
    uint32_t start_time = millis();

    while (!sensorReady()) {
        if ((millis() - start_time) > timeout_ms) {
            logError("Sensor timeout!");
            return false;
        }
        watchdog_refresh();
    }

    return true;
}
```

---

## 📝 Etapas de Modularização

### Passo 1: Análise do Arquivo Original

**Checklist:**
- [ ] Ler arquivo completo
- [ ] Identificar responsabilidades (listar todas)
- [ ] Medir complexidade ciclomática de cada função
- [ ] Identificar violações MISRA
- [ ] Medir níveis de nesting (máx, média)
- [ ] Identificar dependências externas
- [ ] Mapear variáveis globais/static

**Ferramentas:**
```bash
# Contar linhas
wc -l auxiliaries.cpp

# Encontrar funções longas
grep -n "^void\|^bool\|^uint" auxiliaries.cpp

# Encontrar nesting profundo
grep -o "    " auxiliaries.cpp | wc -l  # Contar indentações
```

### Passo 2: Design dos Módulos

**Template de Design:**

```
Nome do Módulo: boost_control
Responsabilidade: Controlar wastegate para boost target
Tamanho estimado: 350 linhas total

Submódulos:
├── boost_openloop.cpp (~100 linhas)
│   Responsabilidade: Lookup de duty cycle em tabela
│   Funções públicas:
│   - uint16_t calculateOpenLoopDuty(uint16_t tps, uint16_t rpm)
│   Funções privadas:
│   - static uint16_t interpolateBoostTable(...)
│
├── boost_closedloop.cpp (~120 linhas)
│   Responsabilidade: PID control do boost
│   Funções públicas:
│   - uint16_t calculateClosedLoopDuty(uint16_t target, uint16_t current)
│   Funções privadas:
│   - static void updateBoostPID(...)
│   - static uint16_t applyPIDLimits(...)
│
└── boost_by_gear.cpp (~150 linhas)
    Responsabilidade: Modificar boost por marcha
    Funções públicas:
    - uint16_t applyGearCompensation(uint16_t baseDuty, uint8_t gear)
    Funções privadas:
    - static uint16_t getGearMultiplier(uint8_t gear)

Dependências:
- currentStatus (leitura: MAP, TPS, RPM, gear)
- configPage9 (leitura: boost tables, PID gains)
- Hardware: TIM1 (PWM output)

Interfaces públicas:
namespace speeduino {
namespace boost {
    void initialise(void);
    void update(void);
    void disable(void);
    uint16_t getDuty(void);
}
}
```

### Passo 3: Criar Estrutura de Diretórios

```bash
# Exemplo para auxiliaries.cpp
mkdir -p speeduino/auxiliaries/air_conditioning
mkdir -p speeduino/auxiliaries/fan_control
mkdir -p speeduino/auxiliaries/boost_control
mkdir -p speeduino/auxiliaries/vvt_control
mkdir -p speeduino/auxiliaries/nitrous_control
mkdir -p speeduino/auxiliaries/wmi_control
```

### Passo 4: Criar Headers com Interfaces

**Template de Header:**

```cpp
/**
 * @file boost_control.h
 * @brief Boost control system interface
 * @details Manages wastegate solenoid for turbocharger boost control
 *
 * @author Speeduino Team
 * @date 2025-01-29
 * @version 1.0
 *
 * @copyright Copyright (c) 2025 Speeduino
 * @license GPL-3.0
 */

#ifndef BOOST_CONTROL_H
#define BOOST_CONTROL_H

#include <stdint.h>
#include <stdbool.h>

namespace speeduino {
namespace boost {

/**
 * @brief Boost control operating modes
 */
enum class BoostMode : uint8_t {
    DISABLED = 0U,      ///< Boost control disabled
    OPEN_LOOP = 1U,     ///< Table-based duty cycle
    CLOSED_LOOP = 2U    ///< PID control to target pressure
};

/**
 * @brief Initialize boost control system
 * @param mode Operating mode for boost control
 * @return true if initialization successful, false otherwise
 * @pre Pin mapping must be completed
 * @post Boost control ready for operation
 * @safety Initializes to safe state (wastegate fully open)
 */
bool initialise(BoostMode mode);

/**
 * @brief Update boost control loop
 * @details Call at 30Hz from main loop
 * @pre Boost control must be initialized
 * @post currentStatus.boostDuty updated
 * @safety Includes overboost protection
 */
void update(void);

/**
 * @brief Disable boost control (emergency)
 * @details Forces wastegate fully open
 * @post Boost duty = 0%, wastegate open
 * @safety Safe to call from any context
 */
void disable(void);

/**
 * @brief Get current boost duty cycle
 * @return Duty cycle in 0.01% units (0-10000 = 0.00%-100.00%)
 */
uint16_t getDuty(void);

/**
 * @brief Get current boost target
 * @return Target boost pressure in kPa
 */
uint16_t getTarget(void);

} // namespace boost
} // namespace speeduino

#endif // BOOST_CONTROL_H
```

### Passo 5: Implementar Módulos (Um de Cada Vez)

**Ordem de implementação:**
1. Começar pelos módulos mais simples
2. Testar cada módulo antes de prosseguir
3. Manter código original comentado inicialmente
4. Usar feature flags para transição gradual

**Template de Implementação:**

```cpp
/**
 * @file boost_openloop.cpp
 * @brief Open-loop boost control implementation
 */

#include "boost_control.h"
#include "globals.h"
#include "table3d.h"

namespace speeduino {
namespace boost {

// Anonymous namespace para funções/variáveis privadas
namespace {

/**
 * @brief Interpolate boost table based on TPS and RPM
 * @param tps Throttle position (0-100%)
 * @param rpm Engine RPM
 * @return Boost duty cycle (0-10000)
 */
static uint16_t interpolateBoostTable(uint16_t tps, uint16_t rpm) {
    // Validação de entrada
    if (tps > 100U) {
        tps = 100U;
    }

    // Lookup em tabela 3D
    uint8_t dutyPercent = get3DTableValue(&boostTable, tps * 2U, rpm);

    // Converter para 0.01% resolution
    return (uint16_t)dutyPercent * 100U;
}

} // anonymous namespace

// Implementação das funções públicas
uint16_t calculateOpenLoopDuty(uint16_t tps, uint16_t rpm) {
    // Guard clauses
    if (configPage4.boostType != OPEN_LOOP_BOOST) {
        return 0U;
    }

    // Cálculo principal
    uint16_t duty = interpolateBoostTable(tps, rpm);

    // Safety clamp
    if (duty > 10000U) {
        logWarning("Boost duty clamped to 100%");
        duty = 10000U;
    }

    return duty;
}

} // namespace boost
} // namespace speeduino
```

### Passo 6: Refatorar No-Nesting

**Técnica 1: Guard Clauses**

❌ **ANTES:**
```cpp
void processData() {
    if (isValid) {
        if (isReady) {
            if (hasData) {
                // Processar
            }
        }
    }
}
```

✅ **DEPOIS:**
```cpp
void processData() {
    if (!isValid) { return; }
    if (!isReady) { return; }
    if (!hasData) { return; }

    // Processar sem nesting
}
```

**Técnica 2: Extract Function**

❌ **ANTES:**
```cpp
void update() {
    if (mode == MODE_A) {
        // 50 linhas de código
    } else if (mode == MODE_B) {
        // 50 linhas de código
    } else {
        // 50 linhas de código
    }
}
```

✅ **DEPOIS:**
```cpp
void update() {
    switch (mode) {
        case MODE_A:
            handleModeA();
            break;
        case MODE_B:
            handleModeB();
            break;
        default:
            handleDefaultMode();
            break;
    }
}

static void handleModeA() {
    // 50 linhas isoladas
}
```

**Técnica 3: State Machine**

❌ **ANTES:**
```cpp
void interrupt() {
    if (state == 0) {
        if (condition1) {
            if (condition2) {
                // nested logic
            }
        }
    } else if (state == 1) {
        if (condition3) {
            if (condition4) {
                // nested logic
            }
        }
    }
}
```

✅ **DEPOIS:**
```cpp
enum class State : uint8_t {
    IDLE,
    RISING,
    FALLING
};

static State currentState = State::IDLE;

static void handleIdleState() {
    // Flat logic
}

static void handleRisingState() {
    // Flat logic
}

void interrupt() {
    switch (currentState) {
        case State::IDLE:
            handleIdleState();
            break;
        case State::RISING:
            handleRisingState();
            break;
        case State::FALLING:
            handleFallingState();
            break;
    }
}
```

**Técnica 4: Table-Driven**

❌ **ANTES:**
```cpp
void configure() {
    switch (gear) {
        case 1:
            if (mode == OPEN) {
                duty = table1[...];
            } else {
                duty = fixed1;
            }
            break;
        case 2:
            if (mode == OPEN) {
                duty = table2[...];
            } else {
                duty = fixed2;
            }
            break;
        // ... 4 more cases (repetitivo!)
    }
}
```

✅ **DEPOIS:**
```cpp
struct GearConfig {
    uint8_t tableValue;
    uint8_t fixedValue;
};

static const GearConfig gearConfigs[6] = {
    { .tableValue = 100, .fixedValue = 50 },  // Gear 1
    { .tableValue = 110, .fixedValue = 60 },  // Gear 2
    // ...
};

void configure() {
    if (gear < 1 || gear > 6) { return; }

    const GearConfig& config = gearConfigs[gear - 1];

    duty = (mode == OPEN) ? config.tableValue : config.fixedValue;
}
```

### Passo 7: Adicionar Safety Features

**Watchdog:**
```cpp
void updateBoostControl() {
    watchdog_refresh();

    // Timeout check
    static uint32_t lastUpdate = 0;
    if ((millis() - lastUpdate) > BOOST_TIMEOUT_MS) {
        logCritical("Boost control timeout!");
        forceDisable();
        return;
    }

    // Normal update
    boost::update();
    lastUpdate = millis();

    watchdog_refresh();
}
```

**Overboost Protection:**
```cpp
void update() {
    // Safety check first
    if (currentStatus.MAP > MAX_SAFE_BOOST) {
        logCritical("OVERBOOST: ", currentStatus.MAP / 2, " kPa");
        forceDisable();
        BIT_SET(currentStatus.engineProtectStatus, PROTECT_BIT_BOOST);
        return;
    }

    // Normal control
    calculateAndApplyBoost();
}
```

**Bounds Checking:**
```cpp
void setValue(uint16_t value) {
    // Clamp to safe range
    const uint16_t MIN_VALUE = 0U;
    const uint16_t MAX_VALUE = 10000U;

    if (value < MIN_VALUE) {
        logWarning("Value below minimum, clamping");
        value = MIN_VALUE;
    }

    if (value > MAX_VALUE) {
        logWarning("Value above maximum, clamping");
        value = MAX_VALUE;
    }

    boostDuty = value;
}
```

### Passo 8: Testes

**Unit Tests (exemplo com Google Test):**

```cpp
// test_boost_control.cpp

#include <gtest/gtest.h>
#include "boost_control.h"

class BoostControlTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup antes de cada teste
        speeduino::boost::initialise(BoostMode::CLOSED_LOOP);
    }

    void TearDown() override {
        // Cleanup após cada teste
        speeduino::boost::disable();
    }
};

TEST_F(BoostControlTest, InitializesToZeroDuty) {
    uint16_t duty = speeduino::boost::getDuty();
    EXPECT_EQ(duty, 0U);
}

TEST_F(BoostControlTest, OpenLoopCalculatesCorrectly) {
    // Arrange
    currentStatus.TPS = 50;
    currentStatus.RPM = 3500;
    configPage4.boostType = OPEN_LOOP_BOOST;

    // Act
    speeduino::boost::update();

    // Assert
    uint16_t duty = speeduino::boost::getDuty();
    EXPECT_GT(duty, 0U);
    EXPECT_LE(duty, 10000U);
}

TEST_F(BoostControlTest, OverboostTriggersProtection) {
    // Simulate overboost
    currentStatus.MAP = 300 * 2; // 300 kPa (muito alto!)

    speeduino::boost::update();

    uint16_t duty = speeduino::boost::getDuty();
    EXPECT_EQ(duty, 0U); // Deve ter desabilitado
    EXPECT_TRUE(BIT_CHECK(currentStatus.engineProtectStatus, PROTECT_BIT_BOOST));
}

TEST_F(BoostControlTest, DutyCycleClampedToMax) {
    // Tentar setar duty > 100%
    // (função interna, testar via mock)

    uint16_t result = calculateDutyWithClamp(15000); // 150%
    EXPECT_EQ(result, 10000U); // Deve clampar para 100%
}
```

**Integration Tests:**

```cpp
TEST(BoostIntegration, FullControlLoop) {
    // Simular loop completo
    initialiseAll();

    // Simular condições de operação
    currentStatus.RPM = 3500;
    currentStatus.MAP = 150; // 75 kPa
    currentStatus.TPS = 80;

    // Rodar vários ciclos
    for (int i = 0; i < 100; i++) {
        speeduino::boost::update();
        // Verificar estabilidade
    }

    // Verificar resultado final
    uint16_t duty = speeduino::boost::getDuty();
    EXPECT_GT(duty, 0U);
    EXPECT_LT(duty, 10000U);
}
```

**Hardware-in-Loop (HIL):**

```cpp
// Teste com hardware real
void testBoostOnBench() {
    // Configurar hardware
    initBenchSetup();

    // Aplicar estímulos
    setPotentiometer(TPS_POT, 50); // 50% TPS
    setRPMGenerator(3500); // 3500 RPM
    setPressureSensor(100); // 100 kPa

    // Rodar firmware
    for (int i = 0; i < 1000; i++) {
        loop();
        delay(10);
    }

    // Medir saídas
    uint16_t pwmDuty = measurePWM(BOOST_PIN);

    // Verificar
    assert(pwmDuty > 10 && pwmDuty < 90);
}
```

### Passo 9: Documentação

**README do módulo:**

```markdown
# Boost Control Module

## Overview
Manages wastegate solenoid for turbocharger boost control.

## Modes
- **Open Loop**: Table-based duty cycle lookup
- **Closed Loop**: PID control to target pressure

## Configuration
- `configPage4.boostType`: Control mode
- `configPage9.boostKP/KI/KD`: PID gains
- `boostTable`: TPS vs RPM boost duty table

## Safety Features
- Overboost protection (>MAX_SAFE_BOOST)
- Timeout protection (500ms)
- Duty cycle clamping (0-100%)

## Testing
```bash
# Unit tests
make test_boost_control

# Integration test
make test_boost_integration

# HIL test
make test_boost_bench
```

## Performance
- Update rate: 30Hz
- CPU usage: ~2%
- RAM usage: 64 bytes
```

### Passo 10: Code Review Checklist

Antes de considerar completo, verificar:

**Funcionalidade:**
- [ ] Todas as funções originais preservadas
- [ ] Comportamento idêntico ao original
- [ ] Testes passando (unit + integration)
- [ ] Validado em hardware real

**Qualidade de Código:**
- [ ] Max 2-3 níveis de nesting
- [ ] Funções <100 linhas
- [ ] Arquivos <300 linhas
- [ ] Nomes descritivos
- [ ] Zero warnings de compilação

**MISRA Compliance:**
- [ ] Rule 8.2: Protótipos com nomes
- [ ] Rule 8.7: Funções static apropriadas
- [ ] Rule 10.3: Type casting explícito
- [ ] Rule 14.4: Expressões booleanas
- [ ] Rule 15.5: Single exit point (onde aplicável)

**Automotive Safety:**
- [ ] Watchdog refresh em loops longos
- [ ] Bounds checking em todos os arrays
- [ ] Overflow protection em multiplicações
- [ ] Timeout protection em I/O
- [ ] Fail-safe modes definidos

**Documentação:**
- [ ] Doxygen em todas as funções públicas
- [ ] README do módulo completo
- [ ] Exemplos de uso
- [ ] Diagramas de arquitetura

**Performance:**
- [ ] Build time igual ou menor
- [ ] RAM usage igual ou menor
- [ ] Flash usage igual ou menor
- [ ] Timing crítico preservado

---

## 📋 Padrões Obrigatórios

### Estrutura de Arquivos

```
speeduino/
├── auxiliaries/
│   ├── auxiliary_interface.h       // Interface pública
│   ├── auxiliary_coordinator.cpp   // Orquestração
│   ├── boost_control/
│   │   ├── boost_control.h         // Interface do módulo
│   │   ├── boost_openloop.cpp      // Implementação
│   │   ├── boost_closedloop.cpp
│   │   └── boost_interrupt.cpp
│   └── [outros módulos]
```

### Naming Conventions

**Arquivos:**
- Headers: `module_name.h`
- Implementation: `module_name.cpp`
- Test: `test_module_name.cpp`

**Funções:**
- Public: `camelCase` ou `PascalCase`
- Private/Static: `camelCase` com `static`
- Callbacks: `onEventName` ou `handleEventName`

**Variáveis:**
- Global: `camelCase`
- Local: `camelCase`
- Static: `s_variableName`
- Volatile: `v_variableName`
- Constants: `UPPER_CASE` ou `kConstantName`

**Tipos:**
- Structs: `PascalCase`
- Enums: `enum class Name : type`
- Typedefs: `TypeName_t`

### Comentários

**File Header:**
```cpp
/**
 * @file boost_control.cpp
 * @brief Boost control implementation
 * @details Manages wastegate solenoid for turbocharger boost control
 *
 * Supports open-loop (table-based) and closed-loop (PID) control modes.
 * Includes overboost protection and gear-based compensation.
 *
 * @author Speeduino Team
 * @date 2025-01-29
 * @version 1.0
 *
 * @copyright Copyright (c) 2025 Speeduino
 * @license GPL-3.0
 *
 * @see boost_control.h for public interface
 */
```

**Function Header:**
```cpp
/**
 * @brief Calculate boost duty cycle using PID control
 * @details Runs PID loop to achieve target boost pressure
 *
 * @param target Target boost pressure in kPa
 * @param current Current boost pressure in kPa
 * @return Duty cycle in 0.01% units (0-10000)
 *
 * @pre Boost control must be initialized
 * @post currentStatus.boostDuty updated
 *
 * @note Called at 30Hz from main loop
 * @warning Target clamped to configPage9.boostMaxPressure
 *
 * @safety Includes overboost protection
 */
uint16_t calculatePIDDuty(uint16_t target, uint16_t current);
```

**Inline Comments:**
```cpp
// Guard clause: reject invalid input
if (channel < 1U || channel > 8U) {
    logError("Invalid channel: ", channel);
    return;
}

// Calculate duty cycle with overflow protection
const uint32_t MAX_INTERMEDIATE = 100000U;
uint32_t temp = (uint32_t)base * (uint32_t)multiplier;
if (temp > MAX_INTERMEDIATE) {
    logError("Calculation overflow!");
    temp = MAX_INTERMEDIATE;
}
```

---

## ✅ Checklist de Qualidade

### Antes de Iniciar Módulo

- [ ] Arquivo original analisado completamente
- [ ] Responsabilidades identificadas e listadas
- [ ] Design de módulos documentado
- [ ] Interfaces definidas em headers
- [ ] Estrutura de diretórios criada
- [ ] Build system atualizado (CMakeLists.txt ou Makefile)

### Durante Implementação

- [ ] Um módulo por vez
- [ ] Código original preservado (comentado)
- [ ] Testes escritos antes da refatoração (TDD)
- [ ] Compilação sem warnings
- [ ] MISRA compliance verificado
- [ ] No-nesting aplicado (<3 níveis)
- [ ] Safety features adicionados

### Antes de Commit

- [ ] Todos os testes passando
- [ ] Code review realizado
- [ ] Documentação completa
- [ ] Performance validada
- [ ] Checklist de qualidade 100%
- [ ] Testado em hardware real
- [ ] Commit message descritivo

### Commit Message Template

```
tipo(escopo): Breve descrição (max 50 chars)

Descrição detalhada do que foi feito e por quê.
Pode ter múltiplas linhas.

- Modularizado auxiliaries.cpp em 6 módulos
- Reduzido nesting de 5 para 2 níveis
- Adicionado watchdog protection
- Conformidade MISRA C++ 85%

BREAKING CHANGE: Interface boost_control alterada
Antes: boostControl() -> Depois: boost::update()

Refs: #123, #456
```

Tipos: `feat`, `fix`, `refactor`, `docs`, `test`, `style`, `perf`

---

## 🎯 FASE 1: auxiliaries.cpp

### Visão Geral

**Arquivo:** `speeduino/auxiliaries.cpp`
**Linhas:** 1.283
**Responsabilidades:** 6 subsistemas auxiliares
**Duração estimada:** 12-16 horas
**Risco:** 5/10 (médio)
**Benefício:** 9/10 (alto)

### Módulos a Criar

```
speeduino/auxiliaries/
├── auxiliary_coordinator.cpp       (~150 linhas)
│   Orquestra updates de todos auxiliares
│
├── air_conditioning/
│   ├── aircon_control.h           (~40 linhas)
│   ├── aircon_control.cpp         (~120 linhas)
│   │   - initialiseAirCon()
│   │   - airConControl()
│   │   - READ_AIRCON_REQUEST()
│   └── aircon_lockouts.cpp        (~100 linhas)
│       - checkAirConCoolantLockout()
│       - checkAirConTPSLockout()
│       - checkAirConRPMLockout()
│
├── fan_control/
│   ├── fan_control.h              (~30 linhas)
│   ├── fan_digital.cpp            (~60 linhas)
│   │   - Digital on/off control
│   ├── fan_pwm.cpp                (~70 linhas)
│   │   - PWM-based control
│   └── fan_interrupt.cpp          (~20 linhas)
│       - fanInterrupt() ISR
│
├── boost_control/
│   ├── boost_control.h            (~50 linhas)
│   ├── boost_openloop.cpp         (~100 linhas)
│   │   - Table lookup
│   ├── boost_closedloop.cpp       (~120 linhas)
│   │   - PID control
│   ├── boost_by_gear.cpp          (~150 linhas)
│   │   - Gear-based compensation
│   └── boost_interrupt.cpp        (~30 linhas)
│       - boostInterrupt() ISR
│
├── vvt_control/
│   ├── vvt_control.h              (~60 linhas)
│   ├── vvt_openloop.cpp           (~100 linhas)
│   │   - Open loop timing
│   ├── vvt_closedloop.cpp         (~150 linhas)
│   │   - PID control
│   └── vvt_interrupt.cpp          (~120 linhas)
│       - vvtInterrupt() ISR (state machine)
│
├── nitrous_control/
│   ├── nitrous_control.h          (~30 linhas)
│   └── nitrous_control.cpp        (~70 linhas)
│       - nitrousControl()
│       - Stage 1/2 logic
│
└── wmi_control/
    ├── wmi_control.h              (~40 linhas)
    └── wmi_control.cpp            (~80 linhas)
        - wmiControl()
        - Tank level check
```

### Problemas Críticos Identificados

**1. vvtInterrupt() - 5 NÍVEIS DE NESTING (linhas 1152-1263)**

**Complexidade:** ~115 linhas, 5 níveis de aninhamento, difícil manutenção

**Solução:** State Machine Pattern

```cpp
// ANTES (linhas 1152-1263) - MUITO COMPLEXO
void vvtInterrupt(void) {
    if (((vvt1_pwm_state == false) || (vvt1_max_pwm == true)) &&
        ((vvt2_pwm_state == false) || (vvt2_max_pwm == true))) {
        if ((vvt1_pwm_value > 0) && (vvt1_max_pwm == false)) {
            VVT1_PIN_ON();
            vvt1_pwm_state = true;
        }
        if ((vvt2_pwm_value > 0) && (vvt2_max_pwm == false)) {
            VVT2_PIN_ON();
            vvt2_pwm_state = true;
        }
        if ((vvt1_pwm_state == true) && ((vvt1_pwm_value <= vvt2_pwm_value) ||
                                          (vvt2_pwm_state == false))) {
            // 3º nível...
            if (vvt1_pwm_value == vvt2_pwm_value) {
                // 4º nível...
            } else {
                // 4º nível...
            }
        }
        // ... mais 80 linhas profundamente aninhadas
    } else {
        if (nextVVT == 0) {
            if (vvt1_pwm_value < (long)vvt_pwm_max_count) {
                // 3º nível...
                if (vvt2_pwm_state == true) {
                    // 4º nível...
                } else {
                    // 4º nível...
                    if (condition) {
                        // 5º NÍVEL!!!
                    }
                }
            }
        }
    }
}

// DEPOIS - MÁXIMO 2 NÍVEIS
enum class VVTState : uint8_t {
    IDLE,
    VVT1_RISING,
    VVT2_RISING,
    BOTH_RISING,
    VVT1_FALLING,
    VVT2_FALLING,
    BOTH_FALLING
};

static VVTState currentState = VVTState::IDLE;

static void handleIdleState(void) {
    // Guard clauses
    if (vvt1_pwm_value == 0 && vvt2_pwm_value == 0) {
        return;
    }

    // Determinar próximo estado (flat logic)
    const bool vvt1Active = (vvt1_pwm_value > 0) && !vvt1_max_pwm;
    const bool vvt2Active = (vvt2_pwm_value > 0) && !vvt2_max_pwm;

    if (vvt1Active && vvt2Active) {
        startBothVVT();
        currentState = VVTState::BOTH_RISING;
    } else if (vvt1Active) {
        startVVT1();
        currentState = VVTState::VVT1_RISING;
    } else if (vvt2Active) {
        startVVT2();
        currentState = VVTState::VVT2_RISING;
    }
}

static void handleVVT1Rising(void) {
    if (isVVT1Complete()) {
        stopVVT1();
        currentState = VVTState::IDLE;
        return;
    }

    scheduleNextVVT1Event();
}

void vvtInterrupt(void) {
    // Dispatch sem nesting
    switch (currentState) {
        case VVTState::IDLE:
            handleIdleState();
            break;
        case VVTState::VVT1_RISING:
            handleVVT1Rising();
            break;
        case VVTState::VVT2_RISING:
            handleVVT2Rising();
            break;
        case VVTState::BOTH_RISING:
            handleBothRising();
            break;
        case VVTState::VVT1_FALLING:
            handleVVT1Falling();
            break;
        case VVTState::VVT2_FALLING:
            handleVVT2Falling();
            break;
        case VVTState::BOTH_FALLING:
            handleBothFalling();
            break;
        default:
            logError("Invalid VVT state");
            currentState = VVTState::IDLE;
            break;
    }
}
```

**2. boostByGear() - 4 NÍVEIS + CÓDIGO REPETITIVO (linhas 546-684)**

**Problema:** 138 linhas, switch com 6 cases quase idênticos

**Solução:** Table-Driven Pattern

```cpp
// ANTES - 48 linhas de repetição para cada marcha
switch (currentStatus.gear) {
    case 1:
        combinedBoost = (((uint16_t)configPage9.boostByGear1 *
                          (uint16_t)get3DTableValue(&boostTable, ...))) << 2;
        if (combinedBoost <= 10000) {
            currentStatus.boostDuty = combinedBoost;
        } else {
            currentStatus.boostDuty = 10000;
        }
        break;
    case 2:
        // Mesmo código com boostByGear2...
        break;
    // ... 4 more cases
}

// DEPOIS - 20 linhas total
struct GearBoostConfig {
    uint8_t multiplier;
};

static const GearBoostConfig gearConfigs[6] = {
    { .multiplier = configPage9.boostByGear1 },
    { .multiplier = configPage9.boostByGear2 },
    { .multiplier = configPage9.boostByGear3 },
    { .multiplier = configPage9.boostByGear4 },
    { .multiplier = configPage9.boostByGear5 },
    { .multiplier = configPage9.boostByGear6 }
};

uint16_t calculateBoostForGear(uint8_t gear) {
    // Guard clause
    if (gear < 1 || gear > 6) {
        return 0;
    }

    const GearBoostConfig& config = gearConfigs[gear - 1];

    uint16_t tableValue = get3DTableValue(&boostTable,
                                          currentStatus.TPS * 2U,
                                          currentStatus.RPM);
    uint32_t combined = ((uint32_t)config.multiplier * tableValue) << 2;

    return (combined > 10000U) ? 10000U : (uint16_t)combined;
}
```

**3. Variáveis Globais (40+ variáveis file-scope)**

**Problema:** Violação MISRA Rule 8.9

**Solução:** Encapsular em structs/classes

```cpp
// ANTES - 40+ variáveis espalhadas
static long vvt1_pwm_value;
static long vvt2_pwm_value;
volatile unsigned int vvt1_pwm_cur_value;
volatile unsigned int vvt2_pwm_cur_value;
static bool vvt1_pwm_state;
static bool vvt2_pwm_state;
// ... mais 34 variáveis

// DEPOIS - Encapsulado
namespace {  // Anonymous namespace

struct VVTChannel {
    long pwm_value;
    volatile uint16_t cur_value;
    volatile bool pwm_state;
    volatile bool max_pwm;
};

static VVTChannel vvt1;
static VVTChannel vvt2;

}  // namespace
```

### Timeline Detalhado (FASE 1)

**Dia 1-2: Air Conditioning (4 horas)**
- [ ] Criar estrutura de diretórios
- [ ] Criar `aircon_control.h` com interface
- [ ] Implementar `aircon_control.cpp`
- [ ] Implementar `aircon_lockouts.cpp`
- [ ] Testes unitários

**Dia 3: Fan Control (2 horas)**
- [ ] Criar `fan_control.h`
- [ ] Implementar `fan_digital.cpp`
- [ ] Implementar `fan_pwm.cpp`
- [ ] Implementar `fan_interrupt.cpp` (ISR)
- [ ] Testes

**Dia 4-5: Boost Control (6 horas)**
- [ ] Criar `boost_control.h`
- [ ] Implementar `boost_openloop.cpp`
- [ ] Implementar `boost_closedloop.cpp` (PID)
- [ ] Refatorar `boost_by_gear.cpp` (table-driven)
- [ ] Implementar `boost_interrupt.cpp` (ISR)
- [ ] Testes de segurança (overboost)

**Dia 6-7: VVT Control (6 horas)**
- [ ] Criar `vvt_control.h`
- [ ] Implementar `vvt_openloop.cpp`
- [ ] Implementar `vvt_closedloop.cpp` (PID)
- [ ] **CRÍTICO:** Refatorar `vvt_interrupt.cpp` (state machine)
- [ ] Testes de timing (ISR <10µs)

**Dia 8: Nitrous + WMI (2 horas)**
- [ ] Implementar `nitrous_control.cpp`
- [ ] Implementar `wmi_control.cpp`
- [ ] Testes

**Dia 9-10: Integração e Testes (4 horas)**
- [ ] Criar `auxiliary_coordinator.cpp`
- [ ] Testes de integração
- [ ] Validação em hardware real
- [ ] Benchmark de performance
- [ ] Documentação final

### Métricas de Sucesso (FASE 1)

| Métrica | Antes | Depois | ✓ |
|---------|-------|--------|---|
| Arquivos >1000 linhas | 1 | 0 | |
| Max nesting | 5 | 2 | |
| Funções >100 linhas | 3 | 0 | |
| Violações MISRA | ~25 | <5 | |
| ISR execution time | ~15µs | <10µs | |
| Modules created | 1 | 6 | |

---

## 🎯 FASE 2: corrections.cpp

### Visão Geral

**Arquivo:** `speeduino/corrections.cpp`
**Linhas:** 1.123
**Responsabilidades:** Correções de combustível, ignição, dwell, AFR
**Duração estimada:** 10-14 horas
**Risco:** 4/10 (baixo-médio)
**Benefício:** 8/10 (alto)

### Módulos a Criar

```
speeduino/corrections/
├── corrections_coordinator.cpp      (~120 linhas)
│
├── fuel_corrections/
│   ├── fuel_corrections.h           (~50 linhas)
│   ├── fuel_warmup.cpp              (~150 linhas)
│   │   - correctionWUE() - Warmup enrichment
│   │   - correctionASE() - After-start enrichment
│   │   - correctionCranking() - Cranking enrichment
│   ├── fuel_accel.cpp               (~200 linhas)
│   │   - correctionAccel() - MAP/TPS based accel
│   │   - State machine para accel/decel
│   ├── fuel_environmental.cpp       (~120 linhas)
│   │   - correctionIATDensity() - Air density
│   │   - correctionBaro() - Barometric pressure
│   │   - correctionFuelTemp() - Fuel temperature
│   ├── fuel_battery.cpp             (~40 linhas)
│   │   - correctionBatVoltage() - Injector compensation
│   ├── fuel_special.cpp             (~100 linhas)
│   │   - correctionFloodClear()
│   │   - correctionLaunch()
│   │   - correctionFlex() - E85 compensation
│   └── fuel_dfco.cpp                (~80 linhas)
│       - correctionDFCO() - Decel fuel cutoff
│       - correctionDFCOfuel() - DFCO taper
│
├── ignition_corrections/
│   ├── ignition_corrections.h       (~50 linhas)
│   ├── ignition_environmental.cpp   (~120 linhas)
│   │   - correctionIATretard()
│   │   - correctionCLTadvance()
│   │   - correctionWMITiming()
│   ├── ignition_timing.cpp          (~150 linhas)
│   │   - correctionFixedTiming()
│   │   - correctionFlexTiming()
│   │   - correctionIdleAdvance()
│   ├── ignition_protection.cpp      (~180 linhas)
│   │   - correctionSoftRevLimit()
│   │   - correctionKnockTiming()
│   │   - correctionNitrous()
│   └── ignition_special.cpp         (~100 linhas)
│       - correctionSoftLaunch()
│       - correctionSoftFlatShift()
│       - correctionDFCOignition()
│
├── dwell_corrections/
│   ├── dwell_corrections.h          (~30 linhas)
│   └── dwell_calculations.cpp       (~120 linhas)
│       - correctionsDwell()
│       - Battery voltage compensation
│
└── afr_corrections/
    ├── afr_corrections.h            (~40 linhas)
    ├── afr_target.cpp               (~100 linhas)
    │   - calculateAfrTarget()
    │   - Table lookup + corrections
    └── afr_closedloop.cpp           (~150 linhas)
        - correctionAFRClosedLoop()
        - PID control
        - Simple O2 control
```

### Problemas Críticos Identificados

**1. correctionAccel() - COMPLEXIDADE CICLOMÁTICA 22 (linhas 275-469)**

**Problema:** 195 linhas, 4-5 níveis nesting, lógica MAP/TPS duplicada

**Solução:** State Machine + Extract Functions

```cpp
// ANTES - 195 linhas, complexidade 22
uint16_t correctionAccel(void) {
    int16_t accelValue = 100;

    if (configPage2.aeMode == AE_MODE_MAP) {
        MAP_change = getMAPDelta();
        currentStatus.mapDOT = (MICROS_PER_SEC / getMAPDeltaTime()) * MAP_change;
    } else if (configPage2.aeMode == AE_MODE_TPS) {
        TPS_change = (currentStatus.TPS - currentStatus.TPSlast);
        currentStatus.tpsDOT = (TPS_READ_FREQUENCY * TPS_change) / 2;
    }

    if (BIT_CHECK(currentStatus.engine, BIT_ENGINE_ACC) ||
        BIT_CHECK(currentStatus.engine, BIT_ENGINE_DCC)) {
        if (micros() >= currentStatus.AEEndTime) {
            // Clear
        } else {
            // Continue
            if ((configPage2.aeMode == AE_MODE_MAP) && (abs(currentStatus.mapDOT) > activateMAPDOT)) {
                // 3º nível...
            } else if ((configPage2.aeMode == AE_MODE_TPS) && (abs(currentStatus.tpsDOT) > activateTPSDOT)) {
                // 3º nível...
            }
        }
    }

    if (!BIT_CHECK(currentStatus.engine, BIT_ENGINE_ACC) &&
        !BIT_CHECK(currentStatus.engine, BIT_ENGINE_DCC)) {
        if (configPage2.aeMode == AE_MODE_MAP) {
            if (abs(MAP_change) <= configPage2.maeMinChange) {
                // No accel (3º nível)
            } else {
                if (abs(currentStatus.mapDOT) > configPage2.maeThresh) {
                    // Activate (3º nível)
                    if (currentStatus.mapDOT < 0) {
                        // Decel (4º nível)
                    } else {
                        // Accel (4º nível)
                        accelValue = table2D_getValue(...);

                        if (currentStatus.RPM > trueTaperMin) {
                            // RPM taper (5º nível em alguns paths)
                        }

                        if (currentStatus.coolant < temperatureRemoveOffset(...)) {
                            // Cold correction (5º nível)
                        }
                    }
                }
            }
        }
        // ... similar para TPS mode (mais 80 linhas)
    }

    return accelValue;
}

// DEPOIS - 80 linhas total, complexidade <10
enum class AccelState : uint8_t {
    IDLE,
    ACCELERATING,
    DECELERATING
};

static AccelState accelState = AccelState::IDLE;

static void updateAccelSensors(void) {
    if (configPage2.aeMode == AE_MODE_MAP) {
        updateMAPDelta();
    } else {
        updateTPSDelta();
    }
}

static int16_t calculateEnrichment(void) {
    // Guard clause
    if (!isAboveThreshold()) {
        return 100;
    }

    int16_t enrichment = lookupEnrichmentTable();
    enrichment = applyRPMTaper(enrichment);
    enrichment = applyColdCorrection(enrichment);

    return 100 + enrichment;
}

static int16_t handleIdleState(void) {
    if (!isAboveThreshold()) {
        return 100;
    }

    // Detectar aceleração/desaceleração
    if (isAccelerating()) {
        accelState = AccelState::ACCELERATING;
        return calculateEnrichment();
    }

    if (isDecelerating()) {
        accelState = AccelState::DECELERATING;
        return calculateDecelLean();
    }

    return 100;
}

static int16_t handleAcceleratingState(void) {
    if (hasTimedOut()) {
        accelState = AccelState::IDLE;
        return 100;
    }

    return taperEnrichment();
}

uint16_t correctionAccel(void) {
    updateAccelSensors();

    switch (accelState) {
        case AccelState::IDLE:
            return handleIdleState();
        case AccelState::ACCELERATING:
            return handleAcceleratingState();
        case AccelState::DECELERATING:
            return handleDeceleratingState();
        default:
            logError("Invalid accel state");
            accelState = AccelState::IDLE;
            return 100;
    }
}
```

**2. Pipeline Multiplicativo Sem Proteção de Overflow**

**Problema:** `sumCorrections` pode estourar uint32_t

**Solução:** Verificação a cada passo

```cpp
// ANTES - Sem proteção
uint32_t sumCorrections = 100;
sumCorrections = div100(sumCorrections * correctionWUE());
sumCorrections = div100(sumCorrections * correctionASE());
sumCorrections = div100(sumCorrections * correctionCranking());
// ... 10+ multiplicações (pode overflow!)

// DEPOIS - Proteção de overflow
uint32_t sumCorrections = 100U;

sumCorrections = applyBoundedCorrection(sumCorrections, correctionWUE());
sumCorrections = applyBoundedCorrection(sumCorrections, correctionASE());
sumCorrections = applyBoundedCorrection(sumCorrections, correctionCranking());
// ... safe em cada passo

static uint32_t applyBoundedCorrection(uint32_t current, uint16_t correction) {
    const uint32_t MAX_INTERMEDIATE = 200000U; // 2000% limit

    // Early return se sem correção
    if (correction == 100U) {
        return current;
    }

    // Verificar overflow ANTES de multiplicar
    if (current > (UINT32_MAX / correction)) {
        logError("Correction would overflow!");
        return MAX_INTERMEDIATE;
    }

    uint32_t result = (current * correction) / 100U;

    // Verificar limite máximo
    if (result > MAX_INTERMEDIATE) {
        logWarning("Correction exceeds maximum");
        result = MAX_INTERMEDIATE;
    }

    return result;
}
```

### Timeline Detalhado (FASE 2)

**Dia 1-2: Fuel Corrections (4 horas)**
- [ ] Criar estrutura
- [ ] `fuel_warmup.cpp` (WUE, ASE, cranking)
- [ ] `fuel_environmental.cpp` (IAT, baro, fuel temp)
- [ ] `fuel_battery.cpp`
- [ ] Testes

**Dia 3-4: Fuel Accel (Complex) (4 horas)**
- [ ] **CRÍTICO:** Refatorar `correctionAccel()` (state machine)
- [ ] `fuel_accel.cpp` com <10 complexidade
- [ ] `fuel_special.cpp` (flood, launch, flex)
- [ ] `fuel_dfco.cpp`
- [ ] Testes extensivos

**Dia 5: Ignition Corrections (2 horas)**
- [ ] `ignition_environmental.cpp`
- [ ] `ignition_timing.cpp`
- [ ] `ignition_protection.cpp` (rev, knock)
- [ ] `ignition_special.cpp`
- [ ] Testes

**Dia 6: Dwell + AFR (2 horas)**
- [ ] `dwell_calculations.cpp`
- [ ] `afr_target.cpp`
- [ ] `afr_closedloop.cpp` (PID)
- [ ] Testes

**Dia 7-8: Integração e Testes (4 horas)**
- [ ] `corrections_coordinator.cpp`
- [ ] Testes A/B (comparar com original)
- [ ] Validação em hardware
- [ ] Documentação

---

## 🎯 FASE 3: init.cpp

### Visão Geral

**Arquivo:** `speeduino/init.cpp`
**Linhas:** 3.892
**Responsabilidades:** 23 subsistemas de inicialização
**Duração estimada:** 24-32 horas
**Risco:** 7/10 (alto - crítico boot path)
**Benefício:** 10/10 (transformacional)

### ⚠️ ATENÇÃO: FASE CRÍTICA

**Este é o arquivo mais crítico do firmware:**
- ❌ Erro aqui = ECU não liga
- ❌ Ordem errada = hardware danificado
- ❌ Pin mapping errado = curto-circuito
- ✅ Testar EXTENSIVAMENTE antes de deploy

### Módulos a Criar

```
speeduino/initialization/
├── init_coordinator.cpp            (~200 linhas)
│   Orquestra sequência completa de boot
│
├── pin_config/
│   ├── pin_mapping_interface.h     (~80 linhas)
│   ├── pin_mapping_stm32f407.cpp   (~250 linhas) APENAS STM32F407
│   └── pin_mapping_registry.cpp    (~100 linhas) Dispatch
│
├── hardware_init/
│   ├── eeprom_reset.cpp            (~40 linhas)
│   ├── serial_setup.cpp            (~60 linhas)
│   ├── timer_config.cpp            (~50 linhas)
│   ├── can_setup.cpp               (~40 linhas)
│   └── sd_rtc_setup.cpp            (~30 linhas)
│
├── engine_config/
│   ├── cylinder_configuration.cpp  (~600 linhas)
│   │   Switch de cilindros (1-12)
│   ├── injection_configuration.cpp (~300 linhas)
│   │   Modo de injeção (sequencial, semi, batch)
│   ├── ignition_configuration.cpp  (~400 linhas)
│   │   Modo de ignição (wasted, sequencial, rotary)
│   └── fuel_calculations.cpp       (~100 linhas)
│       req_fuel_uS, staging
│
├── trigger_init/
│   ├── trigger_setup_interface.h   (~60 linhas)
│   ├── trigger_registry.cpp        (~120 linhas)
│   └── implementations/
│       ├── missing_tooth_setup.cpp (~80 linhas)
│       └── ... (adicionar conforme necessário)
│
└── peripheral_init/
    ├── io_initial_state.cpp        (~80 linhas)
    │   Coils/injectors off
    ├── sensor_calibration.cpp      (~60 linhas)
    │   Load calibration tables
    ├── interrupt_setup.cpp         (~100 linhas)
    │   Flex, VSS, Knock interrupts
    └── pump_tacho_setup.cpp        (~50 linhas)
        Fuel pump priming, tacho sweep
```

### Problemas Críticos Identificados

**1. setPinMapping() - MONSTRUOSO 1.853 LINHAS (47% do arquivo!)**

**Problema:** 70+ configurações de placas, só precisa STM32F407

**Impacto:** Elimina ~1.200 linhas de código não usado (-65%)

```cpp
// ANTES - setPinMapping() com 70+ cases
void setPinMapping(byte boardID) {
    switch(boardID) {
        case 0: // Arduino Mega 2560
            // 180 linhas
            break;
        case 1: // Teensy 3.5
            // 150 linhas
            break;
        // ... 68 more cases
        case 40: // STM32F407 (O QUE PRECISAMOS!)
            // 250 linhas
            break;
        // ... 30 more cases
        default:
            // error
            break;
    }
}

// DEPOIS - APENAS STM32F407
// pin_mapping_stm32f407.cpp (~250 linhas)
namespace speeduino {
namespace pinmapping {

void configureSTM32F407Pins(void) {
    // Injectors
    pinInjector1 = PD12;
    pinInjector2 = PD13;
    pinInjector3 = PD14;
    pinInjector4 = PD15;
    pinInjector5 = PE0;
    pinInjector6 = PE1;
    pinInjector7 = PE2;
    pinInjector8 = PE3;

    // Coils
    pinCoil1 = PD7;
    pinCoil2 = PD6;
    pinCoil3 = PD5;
    pinCoil4 = PD4;
    pinCoil5 = PD3;
    pinCoil6 = PD2;
    pinCoil7 = PD1;
    pinCoil8 = PD0;

    // Sensors
    pinMAP = PA0;
    pinTPS = PA1;
    pinCLT = PA2;
    pinIAT = PA3;
    pinO2 = PA4;
    pinO2_2 = PA5;
    pinBat = PA6;

    // ... resto da configuração (~200 linhas total)

    // Validation
    if (!validatePinConfiguration()) {
        logCritical("Pin validation failed!");
        enterFailSafeMode(FAIL_PIN_CONFIG);
    }
}

}
}
```

**2. Cylinder Configuration - 6 NÍVEIS DE NESTING (linhas 330-822)**

**Problema:** 492 linhas, deeply nested, switch gigante

**Solução:** Dispatch table + extract functions

```cpp
// ANTES - 6 níveis de nesting
if (sparkMode == IGN_MODE_SEQUENTIAL) {
    if (strokes == FOUR_STROKE) {
        if (nCylinders == 4) {
            if (rotaryType == ROTARY_IGN_FC) {
                if (...) {
                    if (...) {
                        // 6º NÍVEL!!!
                    }
                }
            }
        }
    }
}

// DEPOIS - Max 2 níveis
// cylinder_configuration.cpp

typedef void (*CylinderConfigFunc)(void);

struct CylinderConfig {
    uint8_t cylinders;
    uint8_t sparkMode;
    CylinderConfigFunc configureFunc;
};

static void configure4CylSequential(void) {
    channel3IgnDegrees = 360;
    channel4IgnDegrees = 540;
    CRANK_ANGLE_MAX_IGN = 720;
    maxIgnOutputs = 4;
}

static void configure6CylSequential(void) {
    channel3IgnDegrees = 240;
    channel4IgnDegrees = 360;
    channel5IgnDegrees = 480;
    channel6IgnDegrees = 600;
    CRANK_ANGLE_MAX_IGN = 720;
    maxIgnOutputs = 6;
}

static const CylinderConfig configs[] = {
    { 4, IGN_MODE_SEQUENTIAL, configure4CylSequential },
    { 6, IGN_MODE_SEQUENTIAL, configure6CylSequential },
    { 8, IGN_MODE_SEQUENTIAL, configure8CylSequential },
    { 4, IGN_MODE_WASTED, configure4CylWasted },
    // ... etc
};

void configureCylinders(void) {
    // Guard clauses
    if (configPage2.strokes != FOUR_STROKE) {
        configure2Stroke();
        return;
    }

    // Buscar configuração
    for (const auto& config : configs) {
        if (config.cylinders == configPage2.nCylinders &&
            config.sparkMode == configPage4.sparkMode) {
            config.configureFunc();
            return;
        }
    }

    // Fallback
    logError("Unknown cylinder configuration");
    configureDefault();
}
```

**3. Watchdog e Fail-Safe Modes**

**CRÍTICO:** Adicionar proteção de boot

```cpp
// init_coordinator.cpp

#define INIT_TIMEOUT_MS 10000U  // 10s máximo total

void initialiseAll(void) {
    uint32_t init_start = millis();

    // Start watchdog
    watchdog_start(INIT_TIMEOUT_MS);

    // Phase 1: Hardware (CRITICAL)
    if (!initialiseHardware()) {
        logCritical("Hardware init failed!");
        enterFailSafeMode(FAIL_HARDWARE_INIT);
        return; // CANNOT PROCEED
    }
    watchdog_refresh();

    // Phase 2: Pin Mapping (CRITICAL)
    if (!initialisePinMapping()) {
        logCritical("Pin mapping failed!");
        enterFailSafeMode(FAIL_PIN_CONFIG);
        return; // CANNOT PROCEED
    }
    watchdog_refresh();

    // Phase 3: Engine Config (CRITICAL)
    if (!initialiseEngineConfig()) {
        logCritical("Engine config failed!");
        enterFailSafeMode(FAIL_ENGINE_CONFIG);
        return; // CANNOT PROCEED
    }
    watchdog_refresh();

    // Phase 4: Triggers (CRITICAL)
    if (!initialiseTriggers()) {
        logCritical("Trigger init failed!");
        enterFailSafeMode(FAIL_TRIGGER_INIT);
        return; // CANNOT PROCEED
    }
    watchdog_refresh();

    // Phase 5: Peripherals (NON-CRITICAL)
    if (!initialisePeripherals()) {
        logWarning("Peripheral init failed - degraded mode");
        // Continue anyway
    }

    // Success
    currentStatus.initialisationComplete = true;
    watchdog_stop();

    uint32_t init_time = millis() - init_start;
    logInfo("Init complete in ", init_time, "ms");
}

void enterFailSafeMode(FailureCode code) {
    // Disable all outputs
    disableAllInjectors();
    disableAllCoils();
    disableAllAuxiliaries();

    // Set error LED
    setErrorLED(code);

    // Log error
    logCritical("FAIL-SAFE MODE: ", code);

    // Halt
    while (1) {
        watchdog_refresh();
        flashErrorCode(code);
    }
}
```

### Timeline Detalhado (FASE 3)

**Dia 1-3: Pin Mapping (6 horas)**
- [ ] **CRÍTICO:** Extrair apenas STM32F407 config
- [ ] Eliminar 1.200 linhas não usadas
- [ ] Criar pin validation
- [ ] Testes em hardware real

**Dia 4-6: Initialization Sequence (6 horas)**
- [ ] Criar `init_coordinator.cpp` com watchdog
- [ ] Separar hardware_init modules
- [ ] Fail-safe modes
- [ ] Timeout protection

**Dia 7-8: Trigger Setup (4 horas)**
- [ ] Extrair trigger registry
- [ ] Implementations separadas
- [ ] Testes

**Dia 9-10: Cylinder Configuration (4 horas)**
- [ ] **CRÍTICO:** Refatorar de 6 para 2 níveis
- [ ] Dispatch table
- [ ] Testes para cada configuração

**Dia 11-14: Testes Extensivos (8 horas)**
- [ ] Unit tests completos
- [ ] Integration tests
- [ ] **HIL (Hardware-in-Loop) obrigatório**
- [ ] Boot time benchmark
- [ ] Stress tests
- [ ] Documentação

---

## 📚 Exemplos de Código

### Exemplo 1: Guard Clauses Pattern

```cpp
// ❌ EVITAR - Nesting profundo
void processValue(int value) {
    if (value > 0) {
        if (value < 100) {
            if (isEnabled) {
                // processar
            }
        }
    }
}

// ✅ USAR - Guard clauses
void processValue(int value) {
    if (value <= 0) { return; }
    if (value >= 100) { return; }
    if (!isEnabled) { return; }

    // processar sem nesting
}
```

### Exemplo 2: Extract Function

```cpp
// ❌ EVITAR - Função longa
void update() {
    // 50 linhas de código
    // ...
}

// ✅ USAR - Funções pequenas
void update() {
    readSensors();
    calculateValues();
    applyOutputs();
}

static void readSensors() {
    // 15 linhas
}

static void calculateValues() {
    // 20 linhas
}

static void applyOutputs() {
    // 15 linhas
}
```

### Exemplo 3: State Machine

```cpp
// ❌ EVITAR - If/else profundo
void process() {
    if (state == 0) {
        if (condition1) {
            if (condition2) {
                // ...
            }
        }
    } else if (state == 1) {
        if (condition3) {
            // ...
        }
    }
}

// ✅ USAR - State machine
enum class State : uint8_t {
    IDLE,
    RUNNING,
    ERROR
};

static State currentState = State::IDLE;

static void handleIdleState() { }
static void handleRunningState() { }
static void handleErrorState() { }

void process() {
    switch (currentState) {
        case State::IDLE:
            handleIdleState();
            break;
        case State::RUNNING:
            handleRunningState();
            break;
        case State::ERROR:
            handleErrorState();
            break;
    }
}
```

### Exemplo 4: Table-Driven

```cpp
// ❌ EVITAR - Switch repetitivo
switch (gear) {
    case 1: value = calc1(); break;
    case 2: value = calc2(); break;
    case 3: value = calc3(); break;
    // ...
}

// ✅ USAR - Table-driven
typedef uint16_t (*CalcFunc)(void);

static const CalcFunc calcFunctions[] = {
    calc1, calc2, calc3, calc4, calc5, calc6
};

if (gear >= 1 && gear <= 6) {
    value = calcFunctions[gear - 1]();
}
```

---

## 🔧 Troubleshooting

### Problema: Compilação Falha Após Refatoração

**Sintoma:** Erros de linkage, símbolos não definidos

**Solução:**
1. Verificar que todos os headers estão incluídos
2. Verificar namespaces
3. Verificar que CMakeLists.txt/Makefile foi atualizado
4. Limpar build: `make clean && make`

### Problema: Comportamento Diferente do Original

**Sintoma:** Testes falhando, valores diferentes

**Solução:**
1. Usar `#ifdef` para manter código original temporariamente
2. Adicionar logs de debug
3. Comparar valores lado-a-lado
4. Verificar ordem de operações (precedência)
5. Verificar type casting

### Problema: Performance Degradada

**Sintoma:** Build time maior, flash maior, RAM maior

**Solução:**
1. Verificar inline de funções pequenas
2. Verificar que static está sendo usado
3. Profile com `-g -pg`
4. Otimizar hot paths
5. Considerar LTO (Link-Time Optimization)

### Problema: MISRA Violations

**Sintoma:** Muitos warnings do static analyzer

**Solução:**
1. Executar `cppcheck --enable=all`
2. Usar `-Wextra -Wpedantic`
3. Documentar justificativa para desvios
4. Refatorar código não-compliant

---

## 📖 Referências

### Documentos Relacionados
- `MODULARIZATION.md` - Processo completo original
- `MISRA_COMPLIANCE.md` - Regras MISRA detalhadas
- `ARQUIVOS_CRITICOS.md` - Análise de criticidade
- `RTOS_MIGRATION.md` - Roadmap RTOS (Fase 4)

### Padrões de Código
- MISRA C:2012 - Automotive safety standard
- ISO 26262 - Functional safety
- AUTOSAR C++ Guidelines

### Ferramentas
- `cppcheck` - Static analysis
- `clang-tidy` - Linter
- `valgrind` - Memory checker
- `gprof` - Profiler
- Google Test - Unit testing framework

---

**Versão:** 1.0
**Data:** 2025-01-29
**Autor:** Speeduino Modularization Team
**Status:** ATIVO - Usar como referência para todas as fases

---

**PRÓXIMOS PASSOS:**
1. ✅ Ler este guia completamente
2. ✅ Preparar ambiente de desenvolvimento
3. ✅ Criar branch git para Fase 1
4. ▶️ **INICIAR FASE 1: auxiliaries.cpp**
