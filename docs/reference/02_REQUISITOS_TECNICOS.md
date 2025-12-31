# REQUISITOS TÉCNICOS - METODOLOGIA Structured Refactoring
## SCG-ECU 2.0 - Modularização e Adaptação Speeduino para STM32F407VGT6

**Versão:** 2.0
**Data:** 2025-12-30
**Status:** DOCUMENTO DE REFERÊNCIA OBRIGATÓRIO

**Projeto Base:** [Speeduino](https://speeduino.com) por Josh Stewart

---

## ÍNDICE

1. [Visão Geral](#visão-geral)
2. [Padrões de Código Obrigatórios](#padrões-de-código-obrigatórios)
3. [Arquitetura Modular](#arquitetura-modular)
4. [Performance e ISR](#performance-e-isr)
5. [MISRA C++ Compliance](#misra-c-compliance)
6. [Estrutura de Arquivos](#estrutura-de-arquivos)
7. [Processo de Validação](#processo-de-validação)
8. [Checklist de Implementação](#checklist-de-implementação)

---

## 1. VISÃO GERAL

### 1.1 Plataforma Alvo

**Hardware:**
- **MCU:** STM32F407VGT6 (ARM Cortex-M4 @ 168MHz)
- **Flash:** 1MB (524KB efetivo)
- **RAM:** 192KB (131KB efetivo)
- **Canais:** 8x8 (8 injeção + 8 ignição)

**Software:**
- **Framework:** Arduino STM32
- **PlatformIO:** Build system
- **Padrão:** MISRA C:2012 (automotivo)

### 1.2 Objetivos da Modularização

1. **100% Preservação de Lógica** - JAMAIS modificar comportamento original
2. **RTOS-Ready** - Preparado para FreeRTOS futuro
3. **Escalabilidade** - Suporta 2-8 cilindros sem recompilação
4. **Manutenibilidade** - Módulos independentes e testáveis
5. **Safety-Critical** - Compliance automotivo (MISRA C++)

---

## 2. PADRÕES DE CÓDIGO OBRIGATÓRIOS

### 2.1 Complexidade Ciclomática

**REGRA:** Máximo 10 por função (ideal: ≤ 7)

❌ **ERRADO:**
```cpp
void processEngine() {
  if (condition1) {
    if (condition2) {
      if (condition3) {
        if (condition4) {
          if (condition5) {
            // 6+ níveis = INACEITÁVEL
          }
        }
      }
    }
  }
}
```

✅ **CORRETO:**
```cpp
void processEngine() {
  // Guard clauses (early returns)
  if (!condition1) { return; }
  if (!condition2) { return; }
  if (!condition3) { return; }

  // Lógica principal (max 2-3 níveis)
  if (condition4) {
    processStep1();
  }
}
```

### 2.2 Níveis de Aninhamento

**REGRA:** Máximo 2-3 níveis (MISRA C++ Rule 6-4-1)

❌ **ERRADO:**
```cpp
void correctionAccel() {
  if (engine.running) {
    if (tps.active) {
      if (map.valid) {
        if (mode == TPS_BASED) {
          if (delta > threshold) {
            // 5 níveis = INACEITÁVEL
          }
        }
      }
    }
  }
}
```

✅ **CORRETO (Guard Clauses):**
```cpp
void correctionAccel() {
  // Nível 1: Guard clauses
  if (!engine.running) { return 100; }
  if (!tps.active) { return 100; }
  if (!map.valid) { return 100; }

  // Nível 2: Lógica principal
  if (mode == TPS_BASED) {
    return calculateTPSAccel();
  }

  return calculateMAPAccel();
}
```

✅ **CORRETO (State Machine):**
```cpp
enum AccelState { IDLE, TPS_ACCEL, MAP_ACCEL, DECAY };

uint16_t correctionAccel() {
  // State machine elimina aninhamento
  switch (accelState) {
    case IDLE:
      return handleIdleState();
    case TPS_ACCEL:
      return handleTPSAccel();
    case MAP_ACCEL:
      return handleMAPAccel();
    case DECAY:
      return handleDecay();
  }

  return 100; // Default: sem correção
}
```

### 2.3 Tamanho de Funções

**REGRA:** Máximo 50 linhas (ideal: 20-30 linhas)

**Exceções:**
- State machines grandes (até 100 linhas se bem estruturadas)
- Tabelas de lookup (até 200 linhas se const e documentadas)

### 2.4 Single Responsibility Principle (SRP)

**REGRA:** Uma função = Uma responsabilidade

❌ **ERRADO:**
```cpp
void processSensors() {
  // Lê sensores
  readTPS();
  readMAP();
  readCLT();

  // Calcula correções
  calculateWUE();
  calculateASE();

  // Aplica limites
  applyLimits();

  // Envia serial
  sendSerialData();

  // FUNÇÃO FAZ TUDO = PÉSSIMO
}
```

✅ **CORRETO:**
```cpp
void updateSensorReadings() {
  readTPS();
  readMAP();
  readCLT();
}

void updateCorrections() {
  calculateWUE();
  calculateASE();
}

void applySafetyLimits() {
  applyLimits();
}

void updateTelemetry() {
  sendSerialData();
}

// Orquestrador
void mainLoop() {
  updateSensorReadings();
  updateCorrections();
  applySafetyLimits();
  updateTelemetry();
}
```

### 2.5 Nomenclatura de Variáveis

**REGRA:** Descritiva, sem abreviações obscuras

❌ **ERRADO:**
```cpp
int t;           // WTF is 't'?
byte c;          // 'c' = correction? count? current?
uint16_t val;    // qual valor?
bool flg;        // flag de quê?
```

✅ **CORRETO:**
```cpp
int engineTemperature;
byte correctionPercentage;
uint16_t targetAFR;
bool isEngineCranking;
```

### 2.6 Magic Numbers

**REGRA:** Sempre usar constantes nomeadas

❌ **ERRADO:**
```cpp
if (rpm > 400) {
  if (clt < 60) {
    correction = 150;
  }
}
```

✅ **CORRETO:**
```cpp
const uint16_t RPM_CRANKING_THRESHOLD = 400;
const int8_t CLT_COLD_ENGINE_C = 60;
const byte CRANKING_ENRICHMENT_PERCENT = 150;

if (rpm > RPM_CRANKING_THRESHOLD) {
  if (clt < CLT_COLD_ENGINE_C) {
    correction = CRANKING_ENRICHMENT_PERCENT;
  }
}
```

### 2.7 Comentários

**REGRA:** Explique "POR QUÊ", não "O QUÊ"

❌ **ERRADO:**
```cpp
// Incrementa x
x++;

// Checa se RPM é maior que 400
if (rpm > 400) {
  // ...
}
```

✅ **CORRETO:**
```cpp
// Protege contra overflow em cálculo de PW
// (micros() overflow em ~70 minutos, pode causar PW negativo)
if (currentTime < lastTime) {
  lastTime = currentTime;
}

// Limite de RPM para prevenir dano mecânico em válvulas
// (motores com molas de fábrica falham > 7200 RPM)
if (rpm > RPM_MECHANICAL_LIMIT) {
  cutFuel();
}
```

### 2.8 Tipos de Dados

**REGRA:** Use tipos explícitos (stdint.h)

❌ **ERRADO:**
```cpp
int rpm;          // 16-bit? 32-bit? Depende do compilador
unsigned temp;    // Quantos bits?
long timestamp;   // 32-bit? 64-bit?
```

✅ **CORRETO:**
```cpp
uint16_t rpm;           // Explicitamente 16-bit unsigned
int8_t temperature;     // Explicitamente 8-bit signed
uint32_t timestamp;     // Explicitamente 32-bit unsigned
```

---

## 3. ARQUITETURA MODULAR

### 3.1 Padrão de Interface

**REGRA:** Todo módulo segue estrutura de interface + implementação

```cpp
// ============================================================================
// MODULE_NAME_interface.h
// ============================================================================

typedef struct {
  void (*setup)(void);
  void (*process)(void);
  uint16_t (*getValue)(void);
  const char* name;
  uint8_t moduleID;
} ModuleInterface;
```

**Exemplo Real (Decoders):**
```cpp
// decoder_interface.h
typedef struct {
  void (*setup)(void);
  void (*primaryISR)(void);      // CRITICAL: <10µs
  void (*secondaryISR)(void);
  void (*thirdISR)(void);
  uint16_t (*getRPM)(void);
  int (*getCrankAngle)(void);
  const char* name;
  uint8_t decoderID;
} DecoderInterface;
```

### 3.2 Padrão de Registry (Tabela de Lookup)

**REGRA:** O(1) lookup via array const

```cpp
// ============================================================================
// MODULE_NAME_registry.cpp
// ============================================================================

static const ModuleInterface moduleRegistry[] = {
  // MODULE 0
  {
    .setup = &module0_setup,
    .process = &module0_process,
    .getValue = &module0_getValue,
    .name = "Module 0",
    .moduleID = 0
  },
  // MODULE 1
  {
    .setup = &module1_setup,
    .process = &module1_process,
    .getValue = &module1_getValue,
    .name = "Module 1",
    .moduleID = 1
  },
  // ... mais módulos
};

const ModuleInterface* getModule(uint8_t moduleID) {
  if (moduleID >= MODULE_COUNT) {
    return NULL;
  }
  return &moduleRegistry[moduleID];  // O(1) direct access
}
```

### 3.3 Padrão de Coordinator

**REGRA:** Coordinator orquestra todos os submódulos

```cpp
// ============================================================================
// MODULE_NAME_coordinator.h
// ============================================================================

void moduleCoordinatorInitialize(void);
void moduleCoordinatorProcess(void);
uint16_t moduleCoordinatorGetValue(void);
bool moduleCoordinatorIsInitialized(void);
```

**Implementação:**
```cpp
// module_coordinator.cpp

namespace {
  // Cached interface pointer (set once, read-only)
  static const ModuleInterface* activeModule = NULL;
  static volatile bool isInitialized = false;
}

void moduleCoordinatorInitialize(void) {
  // Guard clause
  if (isInitialized) { return; }

  // Get module from registry
  uint8_t moduleID = configPage.moduleType;
  activeModule = getModule(moduleID);

  // Fallback to default
  if (activeModule == NULL) {
    activeModule = getModule(DEFAULT_MODULE_ID);
  }

  // Call setup
  if (activeModule != NULL && activeModule->setup != NULL) {
    activeModule->setup();
  }

  isInitialized = true;
}

void moduleCoordinatorProcess(void) {
  // Guard clause
  if (!isInitialized || activeModule == NULL) {
    return;
  }

  // Direct function pointer call (O(1))
  if (activeModule->process != NULL) {
    activeModule->process();
  }
}
```

### 3.4 Estrutura de Diretórios

**REGRA:** Hierarquia modular clara

```
speeduino/
├── auxiliaries/              (Módulo 2 - COMPLETO)
│   ├── auxiliaries_coordinator.h/cpp
│   ├── boost_control/
│   │   ├── boost_control.h/cpp
│   ├── fan_control/
│   ├── vvt_control/
│   └── ... (10 subsistemas)
│
├── decoders/                 (Módulo 3 - COMPLETO)
│   ├── decoder_coordinator.h/cpp
│   ├── decoder_interface.h
│   ├── decoder_registry.h/cpp
│   ├── decoder_declarations.h
│   └── implementations/
│       ├── missing_tooth.cpp
│       ├── dual_wheel.cpp
│       └── ... (28 decoders)
│
├── corrections/              (Módulo 4 - COMPLETO)
│   ├── corrections_coordinator.h/cpp
│   ├── fuel_corrections/
│   │   ├── fuel_corrections.h/cpp
│   ├── ignition_corrections/
│   ├── dwell_corrections/
│   └── afr_corrections/
│
├── board_config/             (Módulo 1 - COMPLETO)
│   ├── board_config.h/cpp
│   ├── board_registry.h/cpp
│   └── pin_mapping/
│       ├── stm32f407_pins.h/cpp
│       └── pin_setup.h/cpp
│
└── (originais preservados)
    ├── corrections.cpp       (100% intacto)
    ├── decoders.cpp         (100% intacto)
    └── init.cpp             (refatorado, backups criados)
```

---

## 4. PERFORMANCE E ISR

### 4.1 ISR Timing Requirements

**REGRA CRÍTICA:** ISRs devem executar em <10µs (ideal: <5µs)

**Frequência Máxima:**
- 10,000 RPM = 166.67 Hz por cilindro
- 8 cilindros = 1,333 Hz total
- Período mínimo = 750µs entre ISRs
- **Overhead máximo permitido: 10µs (1.3% do período)**

### 4.2 ISR Best Practices

✅ **CORRETO:**
```cpp
// CRITICAL: ISR dispatch - 1-2 CPU cycles overhead
void decoderCoordinatorPrimaryISR(void) {
  // Direct function pointer call (NO conditionals)
  if (isInitialized && activeDecoder != NULL && activeDecoder->primaryISR != NULL) {
    activeDecoder->primaryISR();  // O(1)
  }
}
```

❌ **ERRADO:**
```cpp
void triggerISR(void) {
  // Switch-case = múltiplas comparações (lento!)
  switch (decoderType) {
    case DECODER_MISSING_TOOTH:
      triggerPri_missingTooth();
      break;
    case DECODER_DUAL_WHEEL:
      triggerPri_DualWheel();
      break;
    // ... 28 cases = PÉSSIMO para ISR
  }
}
```

### 4.3 Cached Pointers

**REGRA:** Cache function pointers em RAM

```cpp
namespace {
  // Cached pointer (set once during init, read-only from ISRs)
  static const DecoderInterface* activeDecoder = NULL;
}

void decoderCoordinatorInitialize(void) {
  // Set once
  activeDecoder = decoderRegistryGet(decoderID);
}

void ISR_handler(void) {
  // Ultra-fast access (no lookup, no conditionals)
  activeDecoder->primaryISR();
}
```

### 4.4 Atomic Operations

**REGRA:** Use atomic para variáveis compartilhadas

```cpp
#include <SimplyAtomic.h>

volatile uint16_t toothCount;

void ISR_handler(void) {
  ATOMIC() {
    toothCount++;
  }
}

uint16_t getToothCount(void) {
  uint16_t count;
  ATOMIC() {
    count = toothCount;
  }
  return count;
}
```

### 4.5 Stack Usage

**REGRA:** Minimize uso de stack em ISRs

❌ **ERRADO:**
```cpp
void ISR_handler(void) {
  char buffer[256];    // 256 bytes na stack = PERIGOSO
  float calculations[100];  // 400 bytes = STACK OVERFLOW
}
```

✅ **CORRETO:**
```cpp
// Use variáveis globais/estáticas para ISRs
static char isrBuffer[256];

void ISR_handler(void) {
  uint32_t temp = micros();  // Variáveis locais pequenas = OK
  processTimestamp(temp);
}
```

---

## 5. MISRA C++ COMPLIANCE

### 5.1 Regras Críticas

**MISRA C++ Rule 6-4-1:** Guard clauses preferred over deep nesting
**MISRA C++ Rule 8-0-1:** Functions shall have prototype declarations
**MISRA C++ Rule 5-0-3:** Implicit conversions shall not lose information
**MISRA C++ Rule 0-1-6:** No unused variables

### 5.2 Validação MISRA

```bash
# Validar compliance
cppcheck --enable=all --addon=misra.py speeduino/
```

---

## 6. ESTRUTURA DE ARQUIVOS

### 6.1 Header Guards

**REGRA:** Sempre usar include guards

```cpp
#ifndef MODULE_NAME_H
#define MODULE_NAME_H

// Conteúdo do header

#endif // MODULE_NAME_H
```

### 6.2 Ordem de Includes

**REGRA:** Includes em ordem hierárquica

```cpp
// 1. Header correspondente (se .cpp)
#include "module_name.h"

// 2. Headers do projeto (locais)
#include "../globals.h"
#include "submodule.h"

// 3. Headers de bibliotecas
#include <Arduino.h>

// 4. Headers do sistema
#include <stdint.h>
#include <stdbool.h>
```

### 6.3 Documentação de Header

**REGRA:** Todo header tem bloco de documentação

```cpp
/**
 * @file module_name.h
 * @brief Brief description
 *
 * SCG-ECU 2.0 - STM32F407VGT6 8x8
 *
 * Detailed description of what this module does.
 *
 * @note Important notes
 * @warning Critical warnings
 */
```

---

## 7. PROCESSO DE VALIDAÇÃO

### 7.1 Checklist Pré-Commit

**OBRIGATÓRIO** antes de cada commit:

- [ ] Build SUCCESS
- [ ] Zero warnings de compilação
- [ ] Arquivo original intacto (diff com backup)
- [ ] Documentação atualizada
- [ ] Complexidade < 10 por função
- [ ] Níveis de aninhamento ≤ 3
- [ ] Tamanho de funções < 50 linhas
- [ ] ISR timing verificado (se aplicável)
- [ ] Guard clauses implementadas
- [ ] Magic numbers eliminados
- [ ] Tipos explícitos (uint8_t, etc.)

### 7.2 Validação de Build

```bash
# Clean build
pio run -t clean

# Build completo
pio run -e black_F407VE-EEPROM-SPI

# Verificar tamanho
# Flash: <45% (ideal <40%)
# RAM: <20% (ideal <15%)
```

### 7.3 Validação de Lógica

```bash
# Diff com backup (deve ser IDÊNTICO ou apenas modular)
diff original.cpp original.cpp.backup_original

# Se houver diferenças, revisar linha por linha
# Garantir 100% preservação de lógica
```

---

## 8. CHECKLIST DE IMPLEMENTAÇÃO

### 8.1 Novo Módulo (Procedimento Completo)

**FASE 1: ANÁLISE**
- [ ] Ler documentação do módulo
- [ ] Identificar todas as funções
- [ ] Mapear dependências
- [ ] Identificar funções críticas (ISR, timing-sensitive)
- [ ] Calcular complexidade ciclomática
- [ ] Identificar refatorações necessárias

**FASE 2: BACKUP**
- [ ] Criar backup completo (.backup_original)
- [ ] Verificar backup (diff deve ser vazio)
- [ ] Commit backup separadamente

**FASE 3: ARQUITETURA**
- [ ] Criar estrutura de diretórios
- [ ] Criar interface (module_interface.h)
- [ ] Criar registry (module_registry.h/cpp)
- [ ] Criar coordinator (module_coordinator.h/cpp)
- [ ] Criar subdivisões (se necessário)

**FASE 4: IMPLEMENTAÇÃO**
- [ ] Implementar interfaces
- [ ] Implementar registry table
- [ ] Implementar coordinator
- [ ] Adicionar documentação nos headers originais
- [ ] NÃO modificar implementações originais

**FASE 5: BUILD & TEST**
- [ ] Build completo
- [ ] Verificar Flash/RAM
- [ ] Diff com backup
- [ ] Validar 100% preservação
- [ ] Commit modularização

**FASE 6: DOCUMENTAÇÃO**
- [ ] Atualizar IMPLEMENTACAO_MODULARIZACAO_STATUS.md
- [ ] Atualizar PROJETO_SCG_ECU_MASTER_REFERENCE.md
- [ ] Documentar decisões arquiteturais
- [ ] Commit documentação

---

## 9. FERRAMENTAS OBRIGATÓRIAS

### 9.1 Code Analysis

```bash
# Análise estática
cppcheck --enable=all speeduino/

# MISRA compliance
cppcheck --addon=misra.py speeduino/

# Complexity analysis
lizard speeduino/ -C 10 -L 50
```

### 9.2 Build System

```bash
# PlatformIO
pio run -e black_F407VE-EEPROM-SPI

# Verbose output
pio run -v

# Upload
pio run -t upload
```

---

## 10. PRINCÍPIOS FUNDAMENTAIS

### 10.1 Nunca Quebrar

**JAMAIS:**
- Modificar comportamento original
- Remover lógica existente
- Quebrar compatibilidade
- Introduzir regressões

**SEMPRE:**
- Preservar 100% da lógica
- Manter backups completos
- Validar com diff
- Testar extensivamente

### 10.2 RTOS-Ready

**Todo código deve:**
- Ser thread-safe após inicialização
- Usar const para dados imutáveis
- Minimizar shared mutable state
- Usar atomic para acessos compartilhados

### 10.3 Automotive Safety

**Compliance obrigatório:**
- MISRA C:2012
- Fail-safe defaults
- Guard clauses
- Overflow protection
- Watchdog support

---

## RESUMO EXECUTIVO

**3 REGRAS DE OURO:**

1. **100% PRESERVAÇÃO** - JAMAIS modificar lógica original
2. **MODULAR & ESCALÁVEL** - Interface + Registry + Coordinator
3. **PERFORMANCE CRÍTICO** - ISR <10µs, O(1) lookup, cached pointers

**SEMPRE LEMBRAR:**
- Guard clauses > nested ifs
- State machines > deep nesting
- Const interfaces em flash
- Cached pointers em RAM
- MISRA C++ compliance
- Documentar "por quê"

---

**FIM DO DOCUMENTO DE REQUISITOS TÉCNICOS**

**ESTE DOCUMENTO É OBRIGATÓRIO PARA QUALQUER MODIFICAÇÃO NO CÓDIGO SCG-ECU 2.0**
