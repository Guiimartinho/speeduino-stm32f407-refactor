# PLANO DE AÇÃO - REFATORAÇÃO REAL SCG-ECU 2.0

**Data Criação:** 02/11/2025
**Projeto:** SCG-ECU 2.0 - STM32F407VGT6 8x8
**Status Atual:** 35% completo (organizado, mas não refatorado)
**Objetivo:** 100% compliance com REQUISITOS_TECNICOS.md

---

## SUMÁRIO EXECUTIVO

### Situação Atual

Baseado em **VALIDACAO_CODIGO_REAL.md**, o projeto possui:

**✅ Completo (35%):**
- Estrutura de diretórios modular (7 módulos)
- Arquivos de interface criados
- Algumas guard clauses aplicadas

**❌ Pendente (65%):**
- Migração de código para módulos
- Refatoração de funções grandes
- Redução de complexidade ciclomática
- Aplicação completa de guard clauses

### Escopo do Trabalho

**Funções a Refatorar:** ~80 funções
**Linhas de Código:** ~14,285 linhas nos arquivos monolíticos
**Tempo Estimado:** 20-30 semanas (5-7 meses)
**Recursos:** 1 desenvolvedor full-time

---

## ESTRATÉGIA GERAL

### Princípios

1. **Incremental:** Um módulo por vez, uma função por vez
2. **Testável:** Testes antes e depois de cada refatoração
3. **Seguro:** Branch separada, code review obrigatório
4. **Mensurável:** Métricas de compliance a cada commit
5. **Reversível:** Backups e possibilidade de rollback

### Fases

```
FASE A: Análise Completa          (1 semana)   ← PRÓXIMO PASSO
FASE B: Priorização                (3 dias)
FASE C1: Decoders                  (6-8 semanas)
FASE C2: Corrections               (4-6 semanas)
FASE C3: Communications            (4 semanas)
FASE C4: Sensors                   (2-3 semanas)
FASE C5: Schedulers                (2-3 semanas)
FASE C6: Outros Módulos            (2-3 semanas)
FASE D: Validação Final            (1-2 semanas)
```

**Total:** 22-30 semanas

---

## FASE A: ANÁLISE COMPLETA (Semana 1)

### Objetivos

1. Gerar métricas exatas de compliance
2. Mapear 100% das funções a refatorar
3. Identificar dependências entre módulos
4. Estimar esforço por função

### Tarefas

#### A1: Setup Ferramentas (Dia 1)

```bash
# Instalar ferramentas de análise
pip install lizard
sudo apt-get install cppcheck clang-tidy graphviz

# Configurar CI para métricas automáticas
# (adicionar ao GitHub Actions)
```

**Entregável:** Ambiente configurado

#### A2: Análise de Complexidade (Dia 1-2)

```bash
# Executar lizard em todos os arquivos
cd speeduino
lizard *.cpp -l cpp -C 10 -L 50 --csv > ../docs/metrics/complexity_report.csv

# Gerar relatório HTML
lizard *.cpp -l cpp --html > ../docs/metrics/complexity_report.html
```

**Entregável:** `docs/metrics/complexity_report.csv`

#### A3: Mapeamento de Funções (Dia 2-3)

Script para extrair todas as funções:

```bash
#!/bin/bash
# docs/scripts/extract_functions.sh

echo "Arquivo,Função,Linha Início,Linhas,Complexidade,Status" > function_map.csv

for file in speeduino/*.cpp; do
  # Usar ctags para extrair funções
  ctags -x --c++-kinds=f "$file" | while read line; do
    func_name=$(echo "$line" | awk '{print $1}')
    line_num=$(echo "$line" | awk '{print $3}')

    # Calcular linhas (aproximado)
    lines=$(awk -v start="$line_num" '
      NR >= start && /^[a-zA-Z_]/ && NR > start { print NR - start; exit }
    ' "$file")

    # Extrair complexidade do lizard
    complexity=$(grep "$func_name" ../docs/metrics/complexity_report.csv | cut -d',' -f3)

    # Status
    status="OK"
    if [ "$lines" -gt 50 ]; then status="VIOLA_TAMANHO"; fi
    if [ "$complexity" -gt 10 ]; then status="VIOLA_COMPLEXIDADE"; fi

    echo "$(basename $file),$func_name,$line_num,$lines,$complexity,$status"
  done >> function_map.csv
done
```

**Entregável:** `docs/metrics/function_map.csv`

#### A4: Análise de Guard Clauses (Dia 3)

Script para detectar funções sem guard clauses:

```bash
#!/bin/bash
# docs/scripts/check_guards.sh

echo "Arquivo,Função,Linha,Tem Guards" > guard_check.csv

# Para cada função em function_map.csv
while IFS=',' read -r file func line _rest; do
  # Extrair primeiras 10 linhas da função
  first_lines=$(sed -n "${line},$((line+10))p" "speeduino/$file")

  # Verificar padrão de guard clause
  has_guard="NÃO"
  if echo "$first_lines" | grep -q "^\s*if.*return"; then
    has_guard="SIM"
  fi

  echo "$file,$func,$line,$has_guard"
done < function_map.csv > guard_check.csv
```

**Entregável:** `docs/metrics/guard_check.csv`

#### A5: Análise de Aninhamento (Dia 4)

```bash
# Detectar aninhamento profundo (> 3 níveis = 12+ espaços)
grep -r -n "^\s\{12,\}" speeduino/*.cpp > docs/metrics/deep_nesting.txt
```

**Entregável:** `docs/metrics/deep_nesting.txt`

#### A6: Mapa de Migração (Dia 4-5)

Documento definindo onde cada função será migrada:

```
speeduino/corrections.cpp:
  correctionsFuel()           → corrections/fuel_corrections/fuel_corrections.cpp
  correctionWUE()             → corrections/fuel_corrections/wue.cpp
  correctionASE()             → corrections/fuel_corrections/ase.cpp
  correctionAFRClosedLoop()   → corrections/afr_corrections/afr_closed_loop.cpp
  ...

speeduino/decoders.cpp:
  triggerSetup_missingTooth() → decoders/implementations/missing_tooth.cpp
  triggerPri_missingTooth()   → decoders/implementations/missing_tooth.cpp
  ...
```

**Entregável:** `docs/MAPA_MIGRACAO_MODULOS.md`

#### A7: Consolidação e Relatório (Dia 5)

Criar relatório consolidado:

**Entregável:** `docs/ANALISE_COMPLETA_COMPLIANCE.md`

Estrutura:
```markdown
# ANÁLISE COMPLETA DE COMPLIANCE

## Métricas Gerais
- Total funções: X
- Funções OK: Y (Z%)
- Funções violando tamanho: A
- Funções violando complexidade: B
- Funções sem guard clauses: C

## Top 20 Funções Mais Complexas
[tabela]

## Top 20 Funções Maiores
[tabela]

## Mapa de Violações por Módulo
[gráficos e tabelas]

## Dependências Críticas
[grafo de dependências]
```

---

## FASE B: PRIORIZAÇÃO (Dias 6-8)

### Objetivos

1. Ordenar funções por prioridade
2. Definir sprints (2 semanas cada)
3. Alocar funções aos sprints

### Critérios de Priorização

**Fórmula de Score:**
```
Priority Score =
  (Criticidade × 3) +
  (Complexidade / 10 × 2) +
  (Linhas / 50 × 1.5) +
  (Dependentes × 0.5)

Onde:
  Criticidade:
    ISR = 10
    Performance-critical = 7
    Core logic = 5
    Utility = 3

  Complexidade: valor do lizard
  Linhas: número de linhas
  Dependentes: número de funções que chamam esta
```

### Sprints Planejados

#### Sprint 0: Setup (Semana 1-2)
- Configurar ambiente CI/CD
- Criar templates de refatoração
- Setup testes automatizados
- Documentar processo

#### Sprint 1-4: Decoders (Semanas 3-10, 8 semanas)
- **Sprint 1:** Missing Tooth + Basic Distributor (2 sem)
- **Sprint 2:** Dual Wheel + 4G63 (2 sem)
- **Sprint 3:** GM 7X + Nissan 360 (2 sem)
- **Sprint 4:** Decoders restantes (2 sem)

#### Sprint 5-7: Corrections (Semanas 11-16, 6 semanas)
- **Sprint 5:** AFR Corrections (2 sem)
- **Sprint 6:** Fuel Corrections (2 sem)
- **Sprint 7:** Ignition Corrections (2 sem)

#### Sprint 8-9: Communications (Semanas 17-20, 4 semanas)
- **Sprint 8:** Legacy Serial (2 sem)
- **Sprint 9:** CAN + Secondary (2 sem)

#### Sprint 10-11: Sensors (Semanas 21-23, 3 semanas)
- **Sprint 10:** ADC + MAP (1.5 sem)
- **Sprint 11:** O2 + Misc (1.5 sem)

#### Sprint 12: Schedulers (Semanas 24-25, 2 semanas)

#### Sprint 13: Outros (Semanas 26-27, 2 semanas)

#### Sprint 14: Validação Final (Semanas 28-30, 3 semanas)

**Total:** 30 semanas

**Entregável:** `docs/ROADMAP_REFATORACAO_PRIORIZADO.md`

---

## FASE C1: DECODERS (Semanas 3-10)

### Estratégia Específica

**Ordem de Implementação:**
1. missing_tooth (mais usado, ~30% engines)
2. basic_distributor (mais simples, aprendizado)
3. dual_wheel (complexidade média)
4. 4G63 (complexo, mas bem documentado)
5. GM_7X, Nissan_360, etc.

### Template de Refatoração por Decoder

Para cada decoder (ex: missing_tooth):

#### Passo 1: Análise (2-4 horas)

```bash
# Extrair funções do decoder
grep "void trigger.*_missingTooth" speeduino/decoders.cpp

# Output esperado:
# - triggerSetup_missingTooth()      ~36 linhas   ✅ OK
# - triggerPri_missingTooth()        ~138 linhas  ❌ VIOLA
# - triggerSec_missingTooth()        ~20 linhas   ✅ OK
# - getRPM_missingTooth()            ~15 linhas   ✅ OK
# - getCrankAngle_missingTooth()     ~10 linhas   ✅ OK
```

#### Passo 2: Testes Baseline (2-4 horas)

```bash
# Executar testes unitários existentes
platformio test -f test_decoders_missing_tooth

# Capturar saída como baseline
platformio test -f test_decoders_missing_tooth > baseline_test_output.txt

# Se disponível, executar HIL test
# [executar teste com motor real, capturar traces]
```

#### Passo 3: Criar Branch (10 min)

```bash
git checkout -b refactor/decoders-missing-tooth
```

#### Passo 4: Migrar Código (4-8 horas)

Criar arquivo: `speeduino/decoders/implementations/missing_tooth.cpp`

```cpp
/**
 * @file missing_tooth.cpp
 * @brief Missing Tooth Decoder Implementation
 *
 * SCG-ECU 2.0 - STM32F407VGT6 8x8
 *
 * Refactored: 02/11/2025 - Sprint 1
 * Original: speeduino/decoders.cpp:529-703
 *
 * Implements missing tooth decoder (36-1, 60-2, etc.)
 * Commonly used pattern for most aftermarket ECUs
 */

#include "missing_tooth.h"
#include "../decoder_interface.h"
#include "../../globals.h"
#include "../../crankMaths.h"

// ============================================================================
// FORWARD DECLARATIONS (sub-funções privadas)
// ============================================================================

static bool validateTriggerPulse(void);
static void updateToothCount(void);
static bool detectMissingTooth(void);
static void handleMissingToothDetected(void);
static void calculateRPMFromTooth(void);
static void performSyncCheck(void);

// ============================================================================
// SETUP
// ============================================================================

void triggerSetup_missingTooth(void)
{
  // Guard: Invalid configuration
  if (configPage4.triggerTeeth < 2) { return; }
  if (configPage4.triggerMissingTeeth < 1) { return; }

  // Clear decoder state
  BIT_CLEAR(decoderState, BIT_DECODER_IS_SEQUENTIAL);

  // Calculate tooth angle
  triggerToothAngle = 360 / configPage4.triggerTeeth;
  if (configPage4.TrigSpeed == CAM_SPEED) {
    triggerToothAngle = 720 / configPage4.triggerTeeth;
    BIT_SET(decoderState, BIT_DECODER_IS_SEQUENTIAL);
  }

  // Calculate actual teeth and filter time
  triggerActualTeeth = configPage4.triggerTeeth - configPage4.triggerMissingTeeth;
  triggerFilterTime = (MICROS_PER_SEC / (MAX_RPM / 60U * configPage4.triggerTeeth));

  // Setup secondary trigger filter
  if (configPage4.trigPatternSec == SEC_TRIGGER_4_1) {
    triggerSecFilterTime = MICROS_PER_MIN / MAX_RPM / 4U / 2U;
  } else {
    triggerSecFilterTime = (MICROS_PER_SEC / (MAX_RPM / 60U));
  }

  // Initialize state
  BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
  checkSyncToothCount = (configPage4.triggerTeeth) >> 1;
  toothCurrentCount = 0;
  secondaryToothCount = 0;
  thirdToothCount = 0;
  toothOneTime = 0;
  toothOneMinusOneTime = 0;

  // Calculate stall time
  MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/50U) * triggerToothAngle *
                    (configPage4.triggerMissingTeeth + 1U));

  // Setup secondary trigger requirements
  if ((configPage4.TrigSpeed == CRANK_SPEED) &&
      ((configPage4.sparkMode == IGN_MODE_SEQUENTIAL) ||
       (configPage2.injLayout == INJ_SEQUENTIAL) ||
       (configPage6.vvtEnabled > 0))) {
    BIT_SET(decoderState, BIT_DECODER_HAS_SECONDARY);
  } else {
    BIT_CLEAR(decoderState, BIT_DECODER_HAS_SECONDARY);
  }

#ifdef USE_LIBDIVIDE
  divTriggerToothAngle = libdivide::libdivide_s16_gen(triggerToothAngle);
#endif
}

// ============================================================================
// PRIMARY TRIGGER ISR
// ============================================================================

/**
 * @brief Primary trigger interrupt handler
 *
 * Called on every crank tooth detection
 * PERFORMANCE CRITICAL: Must execute in < 10µs on STM32F407 @ 168MHz
 *
 * Original: 138 lines
 * Refactored: 25 lines (main) + sub-functions
 */
void triggerPri_missingTooth(void)
{
  // Capture timestamp immediately
  curTime = micros();
  curGap = curTime - toothLastToothTime;

  // Guard: Reject noise (too fast pulses)
  if (!validateTriggerPulse()) { return; }

  // Mark as valid trigger
  toothCurrentCount++;
  BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER);

  // Guard: Need at least 2 tooth times for gap calculation
  if ((toothLastToothTime == 0) || (toothLastMinusOneToothTime == 0)) {
    toothLastMinusOneToothTime = toothLastToothTime;
    toothLastToothTime = curTime;
    return;
  }

  // Detect and handle missing tooth
  if (detectMissingTooth()) {
    handleMissingToothDetected();
  }

  // Calculate RPM
  calculateRPMFromTooth();

  // Sync check
  performSyncCheck();

  // Update tooth times for next iteration
  toothLastMinusOneToothTime = toothLastToothTime;
  toothLastToothTime = curTime;
}

// ============================================================================
// PRIVATE SUB-FUNCTIONS
// ============================================================================

/**
 * @brief Validate trigger pulse timing
 * @return true if pulse is valid, false if noise
 */
static bool validateTriggerPulse(void)
{
  // Pulses faster than filter time are noise
  // (36-1 wheel at 8000rpm has triggers ~every 200µs)
  return (curGap >= triggerFilterTime);
}

/**
 * @brief Detect missing tooth by comparing current gap with previous
 * @return true if missing tooth detected
 */
static bool detectMissingTooth(void)
{
  // Guard: Need valid last gap
  if (lastGap == 0) {
    lastGap = curGap;
    return false;
  }

  // Missing tooth detection threshold
  // Current gap should be ~2x (for 1 missing) or ~3x (for 2 missing) vs normal
  uint16_t threshold = (lastGap * 3) >> 1;  // 1.5x threshold

  bool isMissing = (curGap > threshold);

  // Update last gap for next comparison
  if (!isMissing) {
    lastGap = curGap;
  }

  return isMissing;
}

/**
 * @brief Handle missing tooth detection (sync point)
 */
static void handleMissingToothDetected(void)
{
  // Reset tooth counter at sync point
  toothCurrentCount = 1;

  // Mark sync achieved
  currentStatus.hasSync = true;

  // Record tooth one timing for RPM calculation
  toothOneMinusOneTime = toothOneTime;
  toothOneTime = curTime;

  // Update revolution counter for sequential
  if (BIT_CHECK(decoderState, BIT_DECODER_IS_SEQUENTIAL)) {
    revolutionOne = !revolutionOne;
  }

  // Trigger scheduler calculations
  triggerToothAngleIsCorrect = true;
}

/**
 * @brief Calculate RPM from tooth timing
 */
static void calculateRPMFromTooth(void)
{
  // Guard: Need sync
  if (!currentStatus.hasSync) { return; }

  // Calculate time for one complete revolution
  unsigned long revolutionTime;

  if (toothOneTime > toothOneMinusOneTime) {
    revolutionTime = toothOneTime - toothOneMinusOneTime;
  } else {
    // Handle overflow
    revolutionTime = (ULONG_MAX - toothOneMinusOneTime) + toothOneTime;
  }

  // Guard: Prevent division by zero
  if (revolutionTime == 0) { return; }

  // RPM = (60 seconds * 1,000,000 µs) / (revolution time in µs)
  // = 60,000,000 / revolutionTime
  currentStatus.RPM = MICROS_PER_MIN / revolutionTime;

  // Apply cam speed correction if needed
  if (configPage4.TrigSpeed == CAM_SPEED) {
    currentStatus.RPM >>= 1;  // Divide by 2
  }
}

/**
 * @brief Perform sync verification
 */
static void performSyncCheck(void)
{
  // If already synced, just verify we don't exceed tooth count
  if (currentStatus.hasSync) {
    if (toothCurrentCount > triggerActualTeeth) {
      // Lost sync - too many teeth before missing
      currentStatus.hasSync = false;
      currentStatus.syncLossCounter++;
    }
    return;
  }

  // Not synced yet - check if we have enough consecutive teeth
  if (toothCurrentCount >= checkSyncToothCount) {
    currentStatus.startRevolutions++;
  }
}

// ============================================================================
// SECONDARY TRIGGER
// ============================================================================

void triggerSec_missingTooth(void)
{
  // Guard: Secondary not configured
  if (!BIT_CHECK(decoderState, BIT_DECODER_HAS_SECONDARY)) { return; }

  curTime2 = micros();
  curGap2 = curTime2 - toothLastSecToothTime;

  // Guard: Reject noise
  if (curGap2 < triggerSecFilterTime) { return; }

  // Valid secondary pulse
  toothLastSecToothTime = curTime2;

  // Determine revolution (for sequential)
  if (currentStatus.hasSync) {
    revolutionOne = true;
    secondaryToothCount++;
  }
}

// ============================================================================
// RPM GETTER
// ============================================================================

uint16_t getRPM_missingTooth(void)
{
  // Guard: No sync = 0 RPM
  if (!currentStatus.hasSync) { return 0; }

  // Guard: Stall detection
  unsigned long timeSinceLastTooth = micros() - toothLastToothTime;
  if (timeSinceLastTooth > MAX_STALL_TIME) {
    currentStatus.hasSync = false;
    return 0;
  }

  return currentStatus.RPM;
}

// ============================================================================
// CRANK ANGLE GETTER
// ============================================================================

int getCrankAngle_missingTooth(void)
{
  // Guard: No sync = 0 angle
  if (!currentStatus.hasSync) { return 0; }

  // Calculate angle based on current tooth
  int angle = (toothCurrentCount - 1) * triggerToothAngle;

  // Add interpolation between teeth based on time
  unsigned long timeSinceToothStart = micros() - toothLastToothTime;
  unsigned long toothDuration = toothLastToothTime - toothLastMinusOneToothTime;

  if (toothDuration > 0) {
    int interpolatedAngle = (timeSinceToothStart * triggerToothAngle) / toothDuration;
    angle += interpolatedAngle;
  }

  return angle;
}
```

**Header:** `speeduino/decoders/implementations/missing_tooth.h`

```cpp
#ifndef MISSING_TOOTH_H
#define MISSING_TOOTH_H

#include <stdint.h>

// Setup function
void triggerSetup_missingTooth(void);

// ISR functions
void triggerPri_missingTooth(void);
void triggerSec_missingTooth(void);

// Getters
uint16_t getRPM_missingTooth(void);
int getCrankAngle_missingTooth(void);

#endif // MISSING_TOOTH_H
```

#### Passo 5: Atualizar Original (30 min)

Modificar `speeduino/decoders.cpp` para incluir o novo arquivo:

```cpp
// No início do arquivo
#include "decoders/implementations/missing_tooth.h"

// Remover implementações antigas (comentar ou deletar)
// void triggerSetup_missingTooth(void) { ... } ← DELETAR
// void triggerPri_missingTooth(void) { ... }   ← DELETAR
// etc.
```

#### Passo 6: Compilar (15 min)

```bash
platformio run

# Verificar warnings
# Deve compilar limpo (0 warnings)
```

#### Passo 7: Testar (2-4 horas)

```bash
# Testes unitários
platformio test -f test_decoders_missing_tooth

# Comparar com baseline
diff baseline_test_output.txt current_test_output.txt
# Deve ser idêntico

# Se disponível, HIL test
# Verificar:
# - RPM accuracy (±1%)
# - Sync stability (0 sync losses em 1 hora)
# - Performance (triggerPri < 10µs)
```

#### Passo 8: Validar Métricas (30 min)

```bash
# Complexidade
lizard speeduino/decoders/implementations/missing_tooth.cpp

# Verificar:
# - Todas funções < 50 linhas ✅
# - Complexidade < 10 ✅
# - NLOC reduzido ✅

# Cppcheck
cppcheck speeduino/decoders/implementations/missing_tooth.cpp

# Verificar 0 erros
```

#### Passo 9: Code Review (1-2 horas)

Checklist:
- [ ] Todas as funções < 50 linhas
- [ ] Complexidade < 10
- [ ] Guard clauses presentes
- [ ] Aninhamento ≤ 3
- [ ] Comentários Doxygen completos
- [ ] Testes passam
- [ ] Performance validada (se ISR)
- [ ] Sem warnings

#### Passo 10: Commit e PR (30 min)

```bash
git add speeduino/decoders/implementations/missing_tooth.*
git add speeduino/decoders.cpp
git commit -m "refactor: missing tooth decoder following REQUISITOS_TECNICOS.md standards

- Migrated from decoders.cpp:529-703
- Reduced triggerPri_missingTooth from 138 to 25 lines
- Extracted 6 sub-functions (all < 50 lines)
- Applied guard clauses throughout
- Reduced cyclomatic complexity from 25 to 5
- Reduced nesting from 5 to 2 levels
- Added comprehensive Doxygen comments
- All tests passing
- Performance validated: triggerPri < 8µs

Related: #issue_number
Phase: C1 Sprint 1
Module: Decoders - Missing Tooth
"

git push origin refactor/decoders-missing-tooth

# Criar PR no GitHub
gh pr create \
  --title "refactor: Missing Tooth Decoder - REQUISITOS compliance" \
  --body "$(cat docs/templates/pr_refactor_template.md)"
```

#### Passo 11: Merge e Tag (15 min)

```bash
# Após aprovação
git checkout main
git merge refactor/decoders-missing-tooth
git tag refactor/decoder-missing-tooth-v1.0
git push --tags
```

#### Passo 12: Atualizar Documentação (1 hora)

Atualizar:
- `IMPLEMENTACAO_MODULARIZACAO_STATUS.md`
- `VALIDACAO_CODIGO_REAL.md`
- `ROADMAP_REFATORACAO_PRIORIZADO.md`

Registrar métricas:
- Linhas antes: 138
- Linhas depois: 25 (main) + 6 sub-funções
- Complexidade antes: 25
- Complexidade depois: 5
- Tempo gasto: X horas
- Issues encontrados: Y

---

### Repetir para Todos os Decoders

Ordem Sprint 1:
1. missing_tooth (2 dias)
2. basic_distributor (1 dia)

Sprint 2-4: Demais 30 decoders

---

## FASE C2-C6: OUTROS MÓDULOS

Seguir mesma estratégia:
1. Análise
2. Testes baseline
3. Migração
4. Refatoração
5. Validação
6. PR
7. Documentação

---

## FASE D: VALIDAÇÃO FINAL (Semanas 28-30)

### Checklist Final

**Código:**
- [ ] 100% funções < 50 linhas
- [ ] 100% complexidade < 10
- [ ] 100% guard clauses aplicadas
- [ ] 100% aninhamento ≤ 3
- [ ] 100% ISRs < 10µs validadas
- [ ] 0 warnings no build
- [ ] 0 erros cppcheck
- [ ] 90%+ MISRA compliance

**Testes:**
- [ ] 100% testes unitários passando
- [ ] 100% testes integração passando
- [ ] HIL tests passando (se aplicável)
- [ ] Regression tests passando

**Documentação:**
- [ ] Todos os módulos documentados
- [ ] README atualizado
- [ ] CHANGELOG completo
- [ ] Métricas finais registradas

**Performance:**
- [ ] RAM usage não aumentou (ou < 5%)
- [ ] Flash usage não aumentou (ou < 10%)
- [ ] Performance mantida ou melhorada
- [ ] Latência ISRs reduzida

---

## TEMPLATES E FERRAMENTAS

### Template de PR

```markdown
## Tipo de Mudança
- [ ] Refatoração (melhoria de código sem mudança de funcionalidade)

## Descrição
Refatoração de [função/módulo] seguindo padrões REQUISITOS_TECNICOS.md

**Arquivo Original:** `speeduino/[arquivo].cpp:[linhas]`
**Destino:** `speeduino/[módulo]/[arquivo].cpp`

## Mudanças
- Migrado código para estrutura modular
- Reduzido tamanho de função de X para Y linhas
- Extraídas Z sub-funções
- Aplicadas guard clauses
- Reduzida complexidade de A para B
- Reduzido aninhamento de C para D níveis

## Métricas

### Antes
- Linhas: X
- Complexidade: A
- Aninhamento: C
- Guard clauses: N/A

### Depois
- Linhas: Y (principal) + Z sub-funções
- Complexidade: B
- Aninhamento: D
- Guard clauses: ✅ Aplicadas

## Testes
- [ ] Testes unitários passam
- [ ] Testes integração passam
- [ ] HIL tests passam (se aplicável)
- [ ] Performance validada (se ISR)

## Checklist
- [ ] Código segue REQUISITOS_TECNICOS.md
- [ ] Comentários Doxygen adicionados
- [ ] Build sem warnings
- [ ] Cppcheck sem erros
- [ ] Documentação atualizada
- [ ] Métricas registradas

## Reviewers
@guiimartinho
```

### Script de Validação Automática

```bash
#!/bin/bash
# docs/scripts/validate_refactoring.sh

set -e

echo "=== VALIDAÇÃO DE REFATORAÇÃO ==="
echo ""

# 1. Build
echo "1. Compilando..."
platformio run
echo "✅ Build OK"
echo ""

# 2. Warnings
echo "2. Verificando warnings..."
warnings=$(platformio run 2>&1 | grep -i "warning" | wc -l)
if [ "$warnings" -eq 0 ]; then
  echo "✅ 0 warnings"
else
  echo "❌ $warnings warnings encontrados"
  exit 1
fi
echo ""

# 3. Testes
echo "3. Executando testes..."
platformio test
echo "✅ Testes OK"
echo ""

# 4. Complexidade
echo "4. Analisando complexidade..."
lizard speeduino/*.cpp speeduino/**/*.cpp -C 10 -L 50 > /tmp/lizard_check.txt
violations=$(grep "WARNING" /tmp/lizard_check.txt | wc -l)
if [ "$violations" -eq 0 ]; then
  echo "✅ Complexidade OK"
else
  echo "❌ $violations violações de complexidade"
  cat /tmp/lizard_check.txt | grep "WARNING"
  exit 1
fi
echo ""

# 5. Cppcheck
echo "5. Executando cppcheck..."
cppcheck --enable=warning,style --error-exitcode=1 speeduino/ 2>/dev/null
echo "✅ Cppcheck OK"
echo ""

# 6. Guard clauses
echo "6. Verificando guard clauses..."
bash docs/scripts/check_guards.sh
echo "✅ Guard clauses OK"
echo ""

echo "==================================="
echo "✅ TODAS AS VALIDAÇÕES PASSARAM"
echo "==================================="
```

---

## TRACKING E MÉTRICAS

### Dashboard de Progresso

Criar dashboard no GitHub Projects:

**Colunas:**
- Backlog
- Em Análise
- Pronto para Refatorar
- Em Refatoração
- Em Revisão
- Testando
- Concluído

**Métricas por Sprint:**
- Funções refatoradas: X/Y
- Linhas migradas: A/B
- Complexidade média: C → D
- Tempo gasto: E horas
- Bugs encontrados: F
- Performance: G%

### Métricas Finais

Ao final do projeto, registrar:

```markdown
## PROJETO COMPLETO - MÉTRICAS FINAIS

### Código
- Funções refatoradas: 80/80 (100%)
- Linhas migradas: 14,285/14,285 (100%)
- Arquivos modularizados: 45/45 (100%)
- Complexidade média: 25 → 5 (-80%)
- Funções < 50 linhas: 100% (era 20%)
- Guard clauses: 100% (era 30%)

### Qualidade
- Warnings: 0 (era ~50)
- Cppcheck errors: 0 (era ~30)
- MISRA compliance: 92% (era ~40%)

### Performance
- ISR latência média: -35%
- RAM usage: +2.3%
- Flash usage: +5.1%
- Build time: +8% (mais arquivos)

### Esforço
- Tempo total: 28 semanas
- Commits: 156
- PRs: 82
- Code reviews: 164
- Bugs encontrados: 23
- Bugs corrigidos: 23

### ROI
- Manutenibilidade: +300%
- Testabilidade: +250%
- Onboarding novos devs: -60% tempo
- Bugs em produção: -45%
```

---

## CONCLUSÃO

Este plano de ação fornece:

1. **Roadmap claro:** 30 semanas bem definidas
2. **Processo repetível:** Template para cada função
3. **Validação rigorosa:** Métricas e testes em cada etapa
4. **Rastreabilidade:** Dashboard e documentação completa

**Próximo Passo Imediato:**

✅ **Iniciar FASE A (Análise Completa) - Semana 1**

```bash
# Comandos para começar:
cd /home/guiito/Documents/1.Projects/speeduino-stm32f407-refactor
git checkout -b analysis/phase-a
pip install lizard
bash docs/scripts/phase_a_analysis.sh
```

---

**FIM DO PLANO DE AÇÃO**

**Versão:** 1.0
**Data:** 02/11/2025
**Autor:** SCG-ECU 2.0 Refactoring Team
**Aprovação Necessária:** Sim (antes de iniciar Fase C)
