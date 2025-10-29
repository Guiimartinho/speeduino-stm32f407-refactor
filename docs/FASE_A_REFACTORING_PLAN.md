# FASE A - REFATORAÇÃO CORRECTIONS.CPP
## Status: 1/5 funções completas

**Data:** 29/10/2025
**Arquivo:** speeduino/corrections.cpp
**Backup:** corrections.cpp.backup_refactor_phaseA

---

## FUNÇÃO 1: correctionAccel() ✅ COMPLETO

**Status:** Refatorado e validado
**Build:** SUCCESS
**Linhas:** 194 → 189 (2.6% redução na função)
**Complexidade:** 22 → 6 (73% redução)
**Aninhamento:** 5 níveis → 2 níveis

### Refatorações Aplicadas:

1. **6 funções helper criadas:**
   - `calculateDOT()` - Calcula rate of change
   - `applyRPMTaper()` - Aplica correção RPM (guard clauses)
   - `applyColdModifier()` - Aplica correção temperatura (guard clauses)
   - `handleDeceleration()` - Processa desaceleração
   - `handleAcceleration()` - Processa aceleração (unificado MAP/TPS)
   - `processNewActivation()` - Ativa nova correção (unificado MAP/TPS)

2. **Magic numbers eliminados:**
   - `AE_NO_CORRECTION = 100`
   - `AE_TIME_MULTIPLIER_US = 10000`
   - `AE_TABLE_DIVISOR = 10`

3. **Duplicação eliminada:**
   - MAP e TPS usavam código quase idêntico (120 linhas duplicadas)
   - Unificados em funções parametrizadas com flag `isMAP`

4. **Guard clauses aplicadas:**
   - RPM taper: 3 níveis de aninhamento → 2 guard clauses
   - Cold modifier: 3 níveis → 2 guard clauses

### Métricas de Build:
- Flash: 202,288 bytes (38.6%) - ESTÁVEL
- RAM: 21,412 bytes (16.3%) - ESTÁVEL
- Warnings: 0
- Build time: 4.88s

---

## FUNÇÃO 2: correctionAFRClosedLoop() ⏳ PENDENTE

**Localização:** corrections.cpp:643-709
**Linhas atuais:** 67
**Complexidade estimada:** 15
**Aninhamento:** 4 níveis

### Problemas Identificados:
1. **Multi-condition gate** - 11 condições ANDed juntas
2. **Algoritmo inline** - PID e simple algorithm misturados
3. **Aninhamento desnecessário** - 4 níveis para simples lean/rich check

### Plano de Refatoração:

```cpp
// Helper: Check if closed-loop conditions are met
static inline bool isClosedLoopActive(void)
{
  // Guard clauses (11 conditions)
  if (currentStatus.coolant < (int)(configPage6.egoTemp - CALIBRATION_TEMPERATURE_OFFSET)) { return false; }
  if (currentStatus.RPM < (configPage6.egoRPM * 100)) { return false; }
  if (currentStatus.TPS >= configPage4.egoTPSMax) { return false; }
  if (currentStatus.O2 < configPage6.ego_min) { return false; }
  if (currentStatus.O2 > configPage6.ego_max) { return false; }
  if (BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK)) { return false; }
  if (currentStatus.runSecs < configPage6.ego_sdelay) { return false; }
  // ... mais 4 condições

  return true;
}

// Helper: Simple algorithm (1% step)
static inline int16_t simpleAFRCorrection(int16_t correction)
{
  if (currentStatus.O2 > currentStatus.afrTarget) {
    // Running lean, add 1%
    return min(correction + 1, (int16_t)configPage6.egoLimit);
  }
  else {
    // Running rich, subtract 1%
    return max(correction - 1, (int16_t)(-configPage6.egoLimit));
  }
}

// Helper: PID algorithm
static inline int16_t pidAFRCorrection(int16_t correction)
{
  egoO2PID.SetTunings(configPage6.egoKP, configPage6.egoKI, configPage6.egoKD);
  int16_t pidOutput = egoO2PID.Compute(currentStatus.O2);
  correction = constrain(pidOutput, -configPage6.egoLimit, configPage6.egoLimit);
  return correction;
}

// Main function - Refactored
int16_t correctionAFRClosedLoop(void)
{
  int16_t correction = 100;

  // Guard: Closed-loop disabled
  if (configPage6.egoType == EGO_TYPE_OFF) { return correction; }

  // Guard: Conditions not met
  if (!isClosedLoopActive()) { return correction; }

  // Select algorithm
  if (configPage6.egoAlgorithm == EGO_ALGORITHM_SIMPLE) {
    correction = simpleAFRCorrection(correction);
  }
  else { // PID
    correction = pidAFRCorrection(correction);
  }

  return correction;
}
```

**Redução esperada:** 67 → 45 linhas (33%)
**Complexidade esperada:** 15 → 5

---

## FUNÇÃO 3: correctionKnockTiming() ⏳ PENDENTE

**Localização:** corrections.cpp:972-1057
**Linhas atuais:** 86
**Complexidade estimada:** 12
**Aninhamento:** 4 níveis

### Problemas Identificados:
1. **Dois algoritmos completamente diferentes** - Digital vs Analog
2. **State management inline** - Knock active/inactive logic scattered
3. **Recovery calculation** - Complex time-based recovery inline

### Plano de Refatoração (Strategy Pattern):

```cpp
// Helper: Digital knock detection
static inline int16_t knockDetectionDigital(void)
{
  int16_t retard = 0;

  // Guard: No knock detected
  if (knockCounter == 0) {
    // Recovery logic
    if ((millis() - knockLastTime) >= configPage10.knock_recoveryTime) {
      if (knockRetardCurrent > 0) {
        knockRetardCurrent -= configPage10.knock_recoveryRate;
      }
    }
    return knockRetardCurrent;
  }

  // Knock detected - progressive retard
  knockRetardCurrent += configPage10.knock_retardAmount;
  knockRetardCurrent = min(knockRetardCurrent, configPage10.knock_maxRetard);
  knockLastTime = millis();
  knockCounter = 0;

  return knockRetardCurrent;
}

// Helper: Analog knock detection
static inline int16_t knockDetectionAnalog(void)
{
  int16_t retard = 0;

  // Guard: Below threshold
  if (currentStatus.knockLevel < configPage10.knock_threshold) {
    // Recovery logic (same as digital)
    if ((millis() - knockLastTime) >= configPage10.knock_recoveryTime) {
      if (knockRetardCurrent > 0) {
        knockRetardCurrent -= configPage10.knock_recoveryRate;
      }
    }
    return knockRetardCurrent;
  }

  // Above threshold - retard based on level
  retard = map(currentStatus.knockLevel,
               configPage10.knock_threshold, 1023,
               0, configPage10.knock_maxRetard);
  knockRetardCurrent = retard;
  knockLastTime = millis();

  return knockRetardCurrent;
}

// Main function - Refactored with Strategy Pattern
int16_t correctionKnockTiming(void)
{
  // Guard: Knock control disabled
  if (configPage10.knock_mode == KNOCK_MODE_OFF) { return 0; }

  // Guard: Engine not running
  if (currentStatus.RPM < configPage2.crankRPM) { return 0; }

  // Strategy pattern - select algorithm
  if (configPage10.knock_mode == KNOCK_MODE_DIGITAL) {
    return knockDetectionDigital();
  }
  else { // KNOCK_MODE_ANALOG
    return knockDetectionAnalog();
  }
}
```

**Redução esperada:** 86 → 60 linhas (30%)
**Complexidade esperada:** 12 → 6

---

## FUNÇÃO 4: correctionIdleAdvance() ⏳ PENDENTE

**Localização:** corrections.cpp:805-847
**Linhas atuais:** 43
**Complexidade estimada:** 10
**Aninhamento:** 4 níveis

### Problemas Identificados:
1. **Multiple nested conditions** - RPM, VSS, TPS/CTPS checks
2. **Idle target calculation** - Inline com clamping
3. **Taper timer** - Activation delay scattered

### Plano de Refatoração (Guard Clauses):

```cpp
int16_t correctionIdleAdvance(void)
{
  // Guard: Idle advance disabled
  if (configPage2.idleAdvEnabled == 0) { return 0; }

  // Guard: RPM above idle threshold
  if (currentStatus.RPM >= configPage2.idleAdvRPM) { return 0; }

  // Guard: Vehicle moving (VSS check)
  if (configPage2.vssMode != 0) {
    if (currentStatus.vss > 0) { return 0; }
  }

  // Guard: TPS not closed (check TPS or CTPS)
  bool throttleClosed = false;
  if (configPage2.useCTPSinIdleAdvance) {
    throttleClosed = (currentStatus.CTPS < configPage2.idleAdvTPS);
  }
  else {
    throttleClosed = (currentStatus.TPS < configPage2.idleAdvTPS);
  }
  if (!throttleClosed) { return 0; }

  // Calculate RPM delta from target
  int16_t idleRPMdelta = currentStatus.RPM - currentStatus.CLIdleTarget;
  idleRPMdelta = constrain(idleRPMdelta,
                           configPage2.idleAdvRPMDelta,
                           -configPage2.idleAdvRPMDelta);

  // Lookup advance amount
  int16_t advanceAmount = table2D_getValue(&idleAdvanceTable, idleRPMdelta);

  // Taper delay logic
  if (configPage2.idleAdvDelay > 0) {
    // ... taper implementation
  }

  return advanceAmount;
}
```

**Redução esperada:** 43 → 30 linhas (30%)
**Complexidade esperada:** 10 → 6

---

## FUNÇÃO 5: correctionASE() ⏳ PENDENTE

**Localização:** corrections.cpp:217-262
**Linhas atuais:** 46
**Complexidade estimada:** 10
**Aninhamento:** 4 níveis

### Problemas Identificados:
1. **Nested timing checks** - Cranking, timer start, duration, taper
2. **Taper calculation** - map() call inline
3. **Overflow protection** - Scattered

### Plano de Refatoração (Simplify Nesting):

```cpp
uint16_t correctionASE(void)
{
  uint16_t ASE = 100;

  // Guard: ASE disabled
  if (configPage2.asePct == 0) { return ASE; }

  // Guard: Currently cranking
  if (BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK)) { return ASE; }

  // Guard: ASE not yet started
  if (currentStatus.ASEValue == 0) {
    // Start ASE timer on first call after cranking
    if (currentStatus.runSecs < 1) {
      currentStatus.ASEValue = 100 + configPage2.asePct;
      currentStatus.ASE_time = 0;
    }
    return ASE;
  }

  // Calculate elapsed time
  uint16_t ASE_duration = configPage2.aseCount * 10; // Stored as tenths
  uint16_t ASE_elapsed = currentStatus.runSecs - currentStatus.ASE_time;

  // Guard: Duration expired
  if (ASE_elapsed >= ASE_duration) {
    currentStatus.ASEValue = 0; // Disable ASE
    return ASE;
  }

  // Calculate taper (100% → 0%)
  uint16_t taperPercent = (ASE_elapsed * 100UL) / ASE_duration;
  ASE = 100 + map(taperPercent, 0, 100, configPage2.asePct, 0);

  return ASE;
}
```

**Redução esperada:** 46 → 35 linhas (24%)
**Complexidade esperada:** 10 → 5

---

## RESUMO FASE A

### Progresso:
- ✅ correctionAccel() - COMPLETO
- ⏳ correctionAFRClosedLoop() - PENDENTE
- ⏳ correctionKnockTiming() - PENDENTE
- ⏳ correctionIdleAdvance() - PENDENTE
- ⏳ correctionASE() - PENDENTE

### Métricas Esperadas (FASE A Completa):

**Antes:**
- Linhas totais: ~436
- Complexidade média: 15.4
- Aninhamento máximo: 5 níveis
- Funções helper: 0

**Depois (Projetado):**
- Linhas totais: ~368 (16% redução)
- Complexidade média: 5.6 (64% redução)
- Aninhamento máximo: 2 níveis
- Funções helper: 15+

### Validação Obrigatória:

Após cada função refatorada:
1. Build SUCCESS
2. RAM/Flash estável (±2%)
3. Zero warnings
4. Diff com backup documentado

---

**PRÓXIMO PASSO:** Aprovar plano e continuar com função 2-5, ou pausar para revisão?
