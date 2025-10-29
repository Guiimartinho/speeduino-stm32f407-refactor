# MÓDULO 7: SCHEDULERS - PLANEJAMENTO COMPLETO
## SCG-ECU 2.0 - STM32F407VGT6 8x8

**Data:** 29/10/2025
**Status:** FASE 1 - ANÁLISE E PLANEJAMENTO COMPLETA
**Complexidade:** MUITO ALTA (Timing-critical ISRs)

---

## 1. ANÁLISE DE CÓDIGO ORIGINAL

### 1.1 Arquivos

| Arquivo | Linhas | Descrição |
|---------|--------|-----------|
| `scheduler.h` | 258 | Structs, prototypes, inline functions |
| `scheduler.cpp` | 692 | Implementações, ISRs |
| **TOTAL** | **950** | **26 funções públicas + 16 ISRs** |

### 1.2 Structs Principais

#### `FuelSchedule` (scheduler.h:176-205)
```cpp
struct FuelSchedule {
  // Hardware references
  counter_t &counter;          // Timer counter register
  compare_t &compare;          // Compare register
  void (&pTimerDisable)();     // Disable function
  void (&pTimerEnable)();      // Enable function

  // Schedule data
  volatile unsigned long duration;      // Pulse width (µs)
  volatile ScheduleStatus Status;       // OFF, PENDING, RUNNING
  volatile COMPARE_TYPE startCompare;   // Start counter value
  void (*pStartFunction)(void);         // Start callback
  void (*pEndFunction)(void);           // End callback
  COMPARE_TYPE nextStartCompare;        // Next schedule start
  volatile bool hasNextSchedule;        // Queue flag
};
```

**8 instâncias:** fuelSchedule1-8

#### `IgnitionSchedule` (scheduler.h:117-151)
```cpp
struct IgnitionSchedule {
  // Hardware references
  counter_t &counter;
  compare_t &compare;
  void (&pTimerDisable)();
  void (&pTimerEnable)();

  // Schedule data
  volatile unsigned long duration;          // Dwell time (µs)
  volatile ScheduleStatus Status;           // OFF, PENDING, RUNNING
  void (*pStartCallback)(void);             // Dwell start callback
  void (*pEndCallback)(void);               // Spark callback
  volatile unsigned long startTime;         // Overdwell protection
  volatile COMPARE_TYPE startCompare;       // Start counter value
  volatile COMPARE_TYPE endCompare;         // End counter value
  COMPARE_TYPE nextStartCompare;            // Next schedule start
  COMPARE_TYPE nextEndCompare;              // Next schedule end
  volatile bool hasNextSchedule;            // Queue flag
  volatile bool endScheduleSetByDecoder;    // Decoder override flag
};
```

**8 instâncias:** ignitionSchedule1-8

#### `ScheduleStatus` enum (scheduler.h:113)
```cpp
enum ScheduleStatus {
  OFF,      // No scheduled plan
  PENDING,  // Scheduled but not started
  STAGED,   // (Not used)
  RUNNING   // Currently executing
};
```

---

### 1.3 Funções Públicas (26 totais)

#### Inicialização e Priming (2 funções)
1. `initialiseSchedulers(void)` - scheduler.cpp:81
   - **LOC:** 132 linhas
   - **Complexidade:** ~8 (múltiplos if/ifdef)
   - **Função:** Inicializa todos schedules (fuel + ignition)
   - **Calls:** reset() 16x, setup callbacks

2. `beginInjectorPriming(void)` - scheduler.cpp:293
   - **LOC:** 30 linhas
   - **Complexidade:** ~5
   - **Função:** Pulsos priming na partida
   - **Calls:** setFuelSchedule() até 8x

#### Scheduling Functions (4 funções inline)
3. `setFuelSchedule(schedule, timeout, duration)` - scheduler.h:210
   - **INLINE:** always_inline
   - **LOC:** 15 linhas
   - **Complexidade:** 3
   - **Calls:** _setFuelScheduleRunning() ou _setFuelScheduleNext()

4. `_setFuelScheduleRunning(schedule, timeout, duration)` - scheduler.cpp:215
   - **LOC:** 15 linhas
   - **Complexidade:** 2
   - **Critical:** Uses noInterrupts()

5. `_setFuelScheduleNext(schedule, timeout, duration)` - scheduler.cpp:231
   - **LOC:** 12 linhas
   - **Complexidade:** 2
   - **Critical:** Queue next pulse

6. `setIgnitionSchedule(schedule, timeout, duration)` - scheduler.h:156
   - **INLINE:** always_inline
   - **LOC:** 15 linhas
   - **Complexidade:** 4
   - **Calls:** _setIgnitionScheduleRunning() ou _setIgnitionScheduleNext()

7. `_setIgnitionScheduleRunning(schedule, timeout, duration)` - scheduler.cpp:244
   - **LOC:** 15 linhas
   - **Complexidade:** 2
   - **Critical:** Uses noInterrupts()

8. `_setIgnitionScheduleNext(schedule, timeout, duration)` - scheduler.cpp:261
   - **LOC:** 11 linhas
   - **Complexidade:** 2
   - **Critical:** Queue next pulse

#### Refresh Function (1 função)
9. `refreshIgnitionSchedule1(unsigned long timeToEnd)` - scheduler.cpp:274
   - **LOC:** 12 linhas
   - **Complexidade:** 2
   - **Função:** Update ignition end time dynamically
   - **Critical:** Uses noInterrupts()

#### Disable Functions (4 funções)
10. `disableFuelSchedule(byte channel)` - scheduler.cpp:576
    - **LOC:** 48 linhas
    - **Complexidade:** 9 (switch 8 cases)
    - **Critical:** Uses noInterrupts()

11. `disableIgnSchedule(byte channel)` - scheduler.cpp:624
    - **LOC:** 46 linhas
    - **Complexidade:** 9 (switch 8 cases)
    - **Critical:** Uses noInterrupts()

12. `disableAllFuelSchedules(void)` - scheduler.cpp:671
    - **LOC:** 11 linhas
    - **Complexidade:** 1
    - **Calls:** disableFuelSchedule() 8x

13. `disableAllIgnSchedules(void)` - scheduler.cpp:682
    - **LOC:** 11 linhas
    - **Complexidade:** 1
    - **Calls:** disableIgnSchedule() 8x

---

### 1.4 ISR Functions (16 funções - CRÍTICAS!)

#### Fuel ISRs (8 funções)
14-21. `fuelSchedule1-8Interrupt(void)` - scheduler.cpp:370-447
   - **LOC:** 3-4 linhas cada
   - **Complexidade:** 1 cada
   - **CRITICAL:** <10µs requirement
   - **Inline call:** fuelScheduleISR(schedule)

#### Ignition ISRs (8 funções)
22-29. `ignitionSchedule1-8Interrupt(void)` - scheduler.cpp:493-574
   - **LOC:** 3-4 linhas cada
   - **Complexidade:** 1 cada
   - **CRITICAL:** <10µs requirement
   - **Inline call:** ignitionScheduleISR(schedule)

---

### 1.5 Internal ISR Functions (2 funções inline)

#### `fuelScheduleISR(FuelSchedule &schedule)` - scheduler.cpp:327
```cpp
static inline __attribute__((always_inline)) void fuelScheduleISR(...)
```
- **LOC:** 30 linhas
- **Complexidade:** 4 (if-else chain)
- **Estados:**
  - PENDING → call pStartFunction(), set RUNNING
  - RUNNING → call pEndFunction(), set OFF (or queue next)
  - OFF → disable timer
- **CRITICAL:** Completely inlined, zero function call overhead

#### `ignitionScheduleISR(IgnitionSchedule &schedule)` - scheduler.cpp:453
```cpp
static inline __attribute__((always_inline)) void ignitionScheduleISR(...)
```
- **LOC:** 36 linhas
- **Complexidade:** 4 (if-else chain)
- **Estados:**
  - PENDING → call pStartCallback(), set RUNNING, record startTime
  - RUNNING → call pEndCallback(), set OFF, update actualDwell (or queue next)
  - OFF → disable timer
- **CRITICAL:** Completely inlined, zero function call overhead

---

### 1.6 Reset Functions (2 funções internas)

30. `reset(FuelSchedule &schedule)` - scheduler.cpp:69
    - **LOC:** 4 linhas
    - **Complexidade:** 1
    - **Função:** Reset schedule to OFF state

31. `reset(IgnitionSchedule &schedule)` - scheduler.cpp:75
    - **LOC:** 4 linhas
    - **Complexidade:** 1
    - **Função:** Reset schedule to OFF state

---

## 2. DEPENDÊNCIAS IDENTIFICADAS

### 2.1 Headers Incluídos
```cpp
#include "globals.h"         // Global variables, status structs
#include "crankMaths.h"      // angleToTimeMicroSecPerDegree()
#include "scheduledIO.h"     // Output pin control
#include "timers.h"          // Timer hardware definitions
#include "schedule_calcs.h"  // Schedule calculation helpers
#include "utilities.h"       // Utility functions
#include "units.h"           // Unit conversions
```

### 2.2 Dependências Funcionais

**De Decoders (Módulo 3):**
- Trigger timing (ISR timing baseado em dentes)
- `angleToTimeMicroSecPerDegree()` conversion

**De Corrections (Módulo 4):**
- Fuel corrections aplicadas antes do schedule
- Ignition corrections aplicadas antes do schedule

**De Sensors (Módulo 5):**
- `currentStatus.coolant` - priming pulse
- `currentStatus.TPS` - flood clear check
- `currentStatus.actualDwell` - dwell averaging

**De Tables (Módulo 6):**
- `PrimingPulseTable` (table2D) - priming lookup

**Para scheduledIO.h:**
- Callbacks executam inj*On(), inj*Off(), ign*On(), ign*Off()

---

## 3. ANÁLISE DE COMPLEXIDADE

### 3.1 Funções por Complexidade

| Complexidade | Funções | Comentário |
|--------------|---------|------------|
| **1** | 12 | ISRs simples, reset(), disable*All() |
| **2-3** | 6 | _set*Running/Next, inline schedules |
| **4** | 3 | fuelScheduleISR(), ignitionScheduleISR(), setIgnitionSchedule() |
| **5** | 1 | beginInjectorPriming() |
| **8** | 1 | initialiseSchedulers() |
| **9** | 2 | disableFuelSchedule(), disableIgnSchedule() |

✅ **Todas funções <10** (compliance ultrathink)

### 3.2 Aninhamento Máximo

- **Máximo observado:** 2-3 níveis
- **Localização:** disableFuelSchedule() switch-case

✅ **Compliance ultrathink** (≤3 níveis)

### 3.3 Tamanho de Funções

- **Maioria:** 3-30 linhas
- **Maior:** initialiseSchedulers() - 132 linhas (mas é repetitiva/simples)

✅ **Aceitável** (única exceção justificável - setup inicial)

---

## 4. DECISÕES ARQUITETURAIS

### 4.1 Pattern Escolhido: **Direct Wrapper (Como Módulo 5)**

**Razões:**
1. ❌ **Não usar Registry/Dispatch (Módulo 3):**
   - Schedulers são hardcoded (8 canais fixos)
   - Não há seleção dinâmica como decoders
   - ISRs precisam ser ULTRA-rápidas (inline)
   - Overhead de function pointer inaceitável

2. ✅ **Usar Direct Wrapper (Módulo 5):**
   - Wrappers diretos com guard clauses
   - ISRs SEM guard clauses (performance)
   - Funções inline preservadas 100%
   - Zero overhead após compiler optimization

### 4.2 Estrutura de Diretórios

```
speeduino/
├── schedulers/
│   ├── scheduler_coordinator.h      (API pública unificada)
│   ├── scheduler_coordinator.cpp    (Coordinator implementation)
│   │
│   ├── fuel_scheduler/
│   │   ├── fuel_scheduler.h         (FuelSchedule struct + prototypes)
│   │   └── fuel_scheduler.cpp       (Fuel scheduling functions)
│   │
│   └── ignition_scheduler/
│       ├── ignition_scheduler.h     (IgnitionSchedule struct + prototypes)
│       └── ignition_scheduler.cpp   (Ignition scheduling functions)
│
└── scheduler.h/cpp                  (PRESERVED 100% - backups criados)
```

### 4.3 Coordinator API

**Inicialização:**
- `schedulerCoordinatorInitialize()` → initialiseSchedulers()
- `schedulerCoordinatorBeginPriming()` → beginInjectorPriming()

**Fuel Scheduling:**
- `schedulerCoordinatorSetFuel(scheduleNum, timeout, duration)` → setFuelSchedule()
- `schedulerCoordinatorDisableFuel(channel)` → disableFuelSchedule()
- `schedulerCoordinatorDisableAllFuel()` → disableAllFuelSchedules()

**Ignition Scheduling:**
- `schedulerCoordinatorSetIgnition(scheduleNum, timeout, duration)` → setIgnitionSchedule()
- `schedulerCoordinatorRefreshIgnition1(timeToEnd)` → refreshIgnitionSchedule1()
- `schedulerCoordinatorDisableIgnition(channel)` → disableIgnSchedule()
- `schedulerCoordinatorDisableAllIgnition()` → disableAllIgnSchedules()

**ISRs (Direct passthrough - NO wrappers):**
- `fuelSchedule1-8Interrupt()` → Permanecem como estão (INLINE)
- `ignitionSchedule1-8Interrupt()` → Permanecem como estão (INLINE)

---

## 5. RISCOS E MITIGAÇÕES

### 5.1 Risco: ISR Timing Overhead

**Descrição:** Adicionar wrappers pode aumentar overhead ISR >10µs

**Mitigação:**
- ✅ ISRs não terão wrappers (permanecem 100% originais)
- ✅ Funções inline preservadas (setFuelSchedule, setIgnitionSchedule)
- ✅ Apenas funções não-críticas têm wrappers
- ✅ Coordinator apenas para API de conveniência

**Validação:** Osciloscópio/logic analyzer (pendente HIL)

### 5.2 Risco: Quebra de Lógica

**Descrição:** Schedulers são timing-critical, qualquer erro = motor não liga

**Mitigação:**
- ✅ 100% preservação de scheduler.cpp (diff validation)
- ✅ Apenas adicionar camada de API, não modificar implementação
- ✅ Backups completos antes de qualquer mudança
- ✅ Build incremental com validação

**Validação:** Build SUCCESS + diff vazio

### 5.3 Risco: Hardware Timers

**Descrição:** Schedulers dependem de hardware timers específicos (FUEL1_COUNTER, IGN1_COMPARE, etc.)

**Mitigação:**
- ✅ Não modificar timer definitions (permanecem em timers.h)
- ✅ FuelSchedule e IgnitionSchedule structs preservadas 100%
- ✅ Apenas wrapper funções públicas

**Validação:** Build SUCCESS

### 5.4 Risco: Interdependências Complexas

**Descrição:** Schedulers são chamados de múltiplos lugares (speeduino.ino, auxiliaries, etc.)

**Mitigação:**
- ✅ Coordinator API mantém compatibilidade 100%
- ✅ Legacy functions preservadas (backward compatibility)
- ✅ Não quebrar includes existentes

**Validação:** Build SUCCESS em todo projeto

---

## 6. ESTRATÉGIA DE IMPLEMENTAÇÃO

### 6.1 FASE 2: Backup (30 min)

✅ **Checklist:**
- [ ] `cp scheduler.h scheduler.h.backup_original`
- [ ] `cp scheduler.cpp scheduler.cpp.backup_original`
- [ ] `diff scheduler.h scheduler.h.backup_original` (deve ser vazio)
- [ ] `diff scheduler.cpp scheduler.cpp.backup_original` (deve ser vazio)
- [ ] Commit de backup separado

### 6.2 FASE 3: Arquitetura Modular (1 dia)

**3.1 Criar Estrutura:**
```bash
mkdir -p speeduino/schedulers/fuel_scheduler
mkdir -p speeduino/schedulers/ignition_scheduler
```

**3.2 Criar Headers:**
- [ ] `schedulers/scheduler_coordinator.h` - API pública
- [ ] `schedulers/fuel_scheduler/fuel_scheduler.h` - FuelSchedule + prototypes
- [ ] `schedulers/ignition_scheduler/ignition_scheduler.h` - IgnitionSchedule + prototypes

**3.3 Modificar scheduler.h:**
- [ ] Adicionar bloco de documentação (+20-30 linhas comentadas)
- [ ] Explicar sistema modular disponível
- [ ] Include do coordinator comentado (opt-in)

### 6.3 FASE 4: Implementação (2 dias)

**4.1 Fuel Scheduler (fuel_scheduler.cpp):**
- [ ] Direct wrappers para funções fuel
- [ ] Preservar FuelSchedule struct
- [ ] Preservar inline functions
- [ ] Guard clauses em funções não-ISR

**4.2 Ignition Scheduler (ignition_scheduler.cpp):**
- [ ] Direct wrappers para funções ignition
- [ ] Preservar IgnitionSchedule struct
- [ ] Preservar inline functions
- [ ] Guard clauses em funções não-ISR

**4.3 Coordinator (scheduler_coordinator.cpp):**
- [ ] API unificada de scheduling
- [ ] Funções de inicialização
- [ ] Funções de disable
- [ ] Guard clauses consistentes
- [ ] ISRs SEM wrappers (performance)

### 6.4 FASE 5: Build & Validação (1 dia)

**5.1 Build:**
```bash
pio run -t clean
pio run -e black_F407VE-EEPROM-SPI
```

**5.2 Validação:**
- [ ] Build SUCCESS
- [ ] Zero warnings
- [ ] Flash <45% (atualmente 38.7%)
- [ ] RAM <20% (atualmente 17.5%)
- [ ] `diff scheduler.cpp scheduler.cpp.backup_original` → VAZIO
- [ ] `diff scheduler.h scheduler.h.backup_original` → apenas +comentários

**5.3 Commit:**
```bash
git add speeduino/schedulers/
git add speeduino/scheduler.h  # Se modificado
git commit -m "Módulo 7 (Schedulers) - Modularização completa"
git push
```

### 6.5 FASE 6: Documentação (1 dia)

**6.1 Atualizar Documentos:**
- [ ] IMPLEMENTACAO_MODULARIZACAO_STATUS.md (Módulo 7: 100%)
- [ ] PROJETO_SCG_ECU_MASTER_REFERENCE.md (Seção Módulo 7)
- [ ] README.md (100% completo - 7/7 módulos)

**6.2 Criar Documentos:**
- [ ] PHASE7_SCHEDULERS_IMPLEMENTATION.md (Detalhes técnicos)
- [ ] REVISAO_COMPLETA_MODULOS_1_7.md (Atualizar)

**6.3 Commit Documentação:**
```bash
git add docs/
git commit -m "Documentação completa Módulo 7"
git push
```

---

## 7. CHECKLIST DE VALIDAÇÃO

### 7.1 Código

✅ **Compliance Ultrathink:**
- [ ] Complexidade < 10 em todas funções
- [ ] Aninhamento ≤ 3 níveis
- [ ] Tamanho funções < 50 linhas (exceto initialise)
- [ ] Guard clauses (exceto ISRs)
- [ ] Tipos explícitos (uint8_t, etc.)
- [ ] Comentários explicam "POR QUÊ"
- [ ] Magic numbers documentados
- [ ] MISRA C++ compliance

✅ **Preservação Lógica:**
- [ ] scheduler.cpp IDÊNTICO (diff vazio)
- [ ] scheduler.h apenas +comentários
- [ ] Structs preservadas 100%
- [ ] Inline functions preservadas 100%
- [ ] ISRs preservadas 100%

### 7.2 Build

✅ **Métricas:**
- [ ] Build SUCCESS
- [ ] Zero warnings
- [ ] Flash <45%
- [ ] RAM <20%
- [ ] Build time razoável (<15s)

### 7.3 Performance (HIL Pendente)

⚠️ **Requer Hardware:**
- [ ] ISR overhead <10µs (osciloscópio)
- [ ] Timing @ 10,000 RPM (bancada)
- [ ] Jitter <0.5° (logic analyzer)

---

## 8. TEMPO ESTIMADO

| Fase | Atividade | Tempo |
|------|-----------|-------|
| 1 | Análise e Planejamento | ✅ COMPLETO |
| 2 | Backup e Preparação | 30 min |
| 3 | Arquitetura Modular | 1 dia |
| 4 | Implementação | 2 dias |
| 5 | Build & Validação | 1 dia |
| 6 | Documentação | 1 dia |

**TOTAL:** 5-6 dias (sem HIL testing)

---

## 9. DECISÕES FINAIS

### 9.1 O que SERÁ modularizado

✅ **Coordinator API:**
- Inicialização (initialiseSchedulers, beginInjectorPriming)
- Scheduling wrappers (convenience)
- Disable functions

✅ **Subsistemas:**
- fuel_scheduler/ - FuelSchedule struct + wrappers
- ignition_scheduler/ - IgnitionSchedule struct + wrappers

### 9.2 O que NÃO SERÁ modificado

❌ **Preservado 100%:**
- scheduler.cpp - implementações originais
- ISRs (fuelSchedule*Interrupt, ignitionSchedule*Interrupt)
- Inline functions (setFuelSchedule, setIgnitionSchedule)
- Internal ISR functions (fuelScheduleISR, ignitionScheduleISR)
- Structs (FuelSchedule, IgnitionSchedule)
- Timer hardware references

❌ **Apenas documentação:**
- scheduler.h - adicionar bloco de docs (+20-30 linhas)

---

## 10. PRÓXIMO PASSO

**Iniciar FASE 2: Backup e Preparação**

Comando:
```bash
# Criar backups
cp scheduler.h scheduler.h.backup_original
cp scheduler.cpp scheduler.cpp.backup_original

# Verificar
diff scheduler.h scheduler.h.backup_original
diff scheduler.cpp scheduler.cpp.backup_original

# Commit
git add scheduler.*.backup_original
git commit -m "Backup original do Módulo 7 (Schedulers)"
git push
```

---

**STATUS:** ✅ FASE 1 COMPLETA - PRONTO PARA FASE 2

**Documento criado por:** Claude Code
**Data:** 29/10/2025
**Aprovação:** Pendente usuário
