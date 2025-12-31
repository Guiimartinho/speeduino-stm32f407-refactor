# RELATÓRIO DE ANÁLISE: Funções Ativas em decoders.cpp
## SCG-ECU 2.0 - Modularização e Adaptação Speeduino para STM32F407VGT6

**Projeto Base:** [Speeduino](https://speeduino.com) por Josh Stewart
**Data:** 2025-12-30

---

## 📊 RESUMO EXECUTIVO

### Estatísticas Gerais do Arquivo
- **Arquivo**: `speeduino/decoders.cpp`
- **Total de linhas**: 6,682
- **Linhas desabilitadas**: 3,641 (54.5%)
- **Linhas ativas**: 3,041 (45.5%)
- **Blocos #if 0**: 14 blocos
- **Funções ativas**: 68 funções

### Estado da Refatoração
O arquivo passou por uma extensa refatoração onde a maioria das implementações foi movida para arquivos separados em `decoders/implementations/`. Mais da metade do código (54.5%) está desabilitado através de blocos `#if 0` com comentários indicando que foi refatorado.

## 🔍 BLOCOS DESABILITADOS (#if 0)

| Linha | Descrição |
|-------|-----------|
| 614   | Missing Tooth - Movido para `implementations/missing_tooth.cpp` |
| 1008  | Dual Wheel - Movido para `implementations/dual_wheel.cpp` |
| 1273  | Basic Distributor - Movido para `implementations/basic_distributor.cpp` |
| 1423  | GM 7X - Movido para `implementations/gm_7x.cpp` |
| 1674  | 4G63 - Movido para `implementations/four_g63.cpp` |
| 2030  | 24X, Jeep2000, Audi135, HondaD17, HondaJ32 - Movidos para implementations/ |
| 2675  | Miata9905, MazdaAU, Non360, Nissan360, Subaru67 - Movidos para implementations/ |
| 3744  | Daihatsu, Harley - Movidos para implementations/ |
| 4039  | ThirtySixMinus222, ThirtySixMinus21, 420a, FordST170 - Movidos para implementations/ |
| 4652  | Weber - Movido para implementations/ |
| 4793  | FordST170 - Movido para implementations/ |
| 4960  | DRZ400, NGC, Vmax, Renix, RoverMEMS, SuzukiK6A - Movidos para implementations/ |
| 5344  | NGC (part 2), Vmax, Renix, RoverMEMS, SuzukiK6A - Movidos para implementations/ |
| 6475  | FordTFI - Movido para implementations/ |

## ⚠️ VIOLAÇÕES MISRA - RESUMO

### Distribuição de Severidade
| Severidade | Quantidade | Percentual | Critérios |
|------------|------------|------------|-----------|
| **CRITICAL** | 10 | 14.7% | N ≥ 5 |
| **HIGH** | 2 | 2.9% | Linhas > 100 OU C > 15 |
| **MEDIUM** | 5 | 7.4% | Linhas > 50 OU C > 10 OU N = 4 |
| **LOW** | 10 | 14.7% | Linhas > 40 OU N = 3 |
| **SEM VIOLAÇÕES** | 41 | 60.3% | Conformidade total |

### Legenda de Métricas
- **N**: Nível máximo de nesting (aninhamento)
- **C**: Complexidade ciclomática (número de caminhos de execução)
- **Lines**: Número de linhas ativas da função

---

## 🚨 VIOLAÇÕES CRÍTICAS (CRITICAL) - 10 Funções

### 1. triggerPri_missingTooth
- **Localização**: Linhas 652-769
- **Métricas**: Lines=118, N=8, C=36
- **Violações**:
  - N=8 (CRITICAL: ≥5)
  - C=36 (HIGH: >15)
  - Lines=118 (HIGH: >100)
- **Prioridade**: MÁXIMA - Pior violação em todas as métricas

### 2. triggerSec_NGC68
- **Localização**: Linhas 5289-5342
- **Métricas**: Lines=54, N=8, C=17
- **Violações**:
  - N=8 (CRITICAL: ≥5)
  - C=17 (HIGH: >15)
  - Lines=54

### 3. triggerPri_NGC
- **Localização**: Linhas 5130-5233
- **Métricas**: Lines=104, N=7, C=25
- **Violações**:
  - N=7 (CRITICAL: ≥5)
  - C=25 (HIGH: >15)
  - Lines=104 (HIGH: >100)

### 4. triggerSec_RoverMEMS
- **Localização**: Linhas 5987-6067
- **Métricas**: Lines=81, N=6, C=18
- **Violações**:
  - N=6 (CRITICAL: ≥5)
  - C=18 (HIGH: >15)
  - Lines=81

### 5. triggerPri_Renix
- **Localização**: Linhas 5665-5737
- **Métricas**: Lines=73, N=5, C=17
- **Violações**:
  - N=5 (CRITICAL: ≥5)
  - C=17 (HIGH: >15)
  - Lines=73

### 6. triggerSec_NGC4
- **Localização**: Linhas 5237-5285
- **Métricas**: Lines=49, N=5, C=17
- **Violações**:
  - N=5 (CRITICAL: ≥5)
  - C=17 (HIGH: >15)
  - Lines=49

### 7. triggerSec_FordTFI
- **Localização**: Linhas 6554-6614
- **Métricas**: Lines=61, N=5, C=15
- **Violações**:
  - N=5 (CRITICAL: ≥5)
  - C=15
  - Lines=61

### 8. triggerPri_RoverMEMS
- **Localização**: Linhas 5867-5924
- **Métricas**: Lines=58, N=5, C=12
- **Violações**:
  - N=5 (CRITICAL: ≥5)
  - C=12
  - Lines=58

### 9. triggerPri_Vmax
- **Localização**: Linhas 5490-5547
- **Métricas**: Lines=58, N=5, C=11
- **Violações**:
  - N=5 (CRITICAL: ≥5)
  - C=11
  - Lines=58

### 10. processSimpleSecTrigger
- **Localização**: Linhas 947-990
- **Métricas**: Lines=44, N=5, C=7
- **Violações**:
  - N=5 (CRITICAL: ≥5)
  - Lines=44

---

## ⚠️ VIOLAÇÕES ALTAS (HIGH) - 2 Funções

### 1. triggerPri_SuzukiK6A
- **Localização**: Linhas 6294-6398
- **Métricas**: Lines=105, N=4, C=19
- **Violações**:
  - N=4
  - C=19 (HIGH: >15)
  - Lines=105 (HIGH: >100)

### 2. triggerSetEndTeeth_RoverMEMS
- **Localização**: Linhas 6089-6148
- **Métricas**: Lines=60, N=3, C=28
- **Violações**:
  - N=3
  - C=28 (HIGH: >15) - Complexidade extremamente alta!
  - Lines=60

---

## ⚡ VIOLAÇÕES MÉDIAS (MEDIUM) - 5 Funções

### 1. triggerPri_DualWheel
- **Localização**: Linhas 1032-1073
- **Métricas**: Lines=42, N=4, C=11
- **Violações**: N=4, C=11, Lines=42

### 2. triggerPri_FordTFI
- **Localização**: Linhas 6504-6549
- **Métricas**: Lines=46, N=4, C=9
- **Violações**: N=4, Lines=46

### 3. triggerSec_missingTooth
- **Localização**: Linhas 773-812
- **Métricas**: Lines=40, N=4, C=6
- **Violações**: N=4

### 4. getRPM_Vmax
- **Localização**: Linhas 5557-5583
- **Métricas**: Lines=27, N=4, C=5
- **Violações**: N=4

### 5. checkPerToothTiming
- **Localização**: Linhas 545-590
- **Métricas**: Lines=46, N=3, C=15
- **Violações**: N=3, C=15, Lines=46

---

## 📋 VIOLAÇÕES BAIXAS (LOW) - 10 Funções

### Funções com Nesting = 3

1. **loggerPrimaryISR**
   - Localização: Linhas 298-327
   - Métricas: Lines=30, N=3, C=10

2. **apply4G63FilterConfig**
   - Localização: Linhas 1637-1672
   - Métricas: Lines=36, N=3, C=8

3. **triggerRoverMEMSCommon**
   - Localização: Linhas 5927-5954
   - Métricas: Lines=28, N=3, C=8

4. **triggerSec_DualWheel**
   - Localização: Linhas 1077-1107
   - Métricas: Lines=31, N=3, C=7

5. **setEndTeethFromDistributorConfig**
   - Localização: Linhas 1241-1269
   - Métricas: Lines=29, N=3, C=6

6. **triggerSec_FordST170**
   - Localização: Linhas 4853-4886
   - Métricas: Lines=34, N=3, C=5

7. **getRPM_missingTooth**
   - Localização: Linhas 848-864
   - Métricas: Lines=17, N=3, C=3

8. **getRPM_FordST170**
   - Localização: Linhas 4888-4904
   - Métricas: Lines=17, N=3, C=3

9. **getRPM_DualWheel**
   - Localização: Linhas 1111-1126
   - Métricas: Lines=16, N=3, C=3

### Função com Excesso de Linhas

10. **triggerSetup_SuzukiK6A**
    - Localização: Linhas 6157-6202
    - Métricas: Lines=46, N=1, C=2
    - Violação: Apenas excesso de linhas

---

## ✅ FUNÇÕES SEM VIOLAÇÕES - 41 Funções

As seguintes 41 funções estão em conformidade com os critérios MISRA:

1. nullTriggerHandler (134-134) - Lines=1, N=0, C=1
2. nullGetRPM (135-135) - Lines=1, N=0, C=1
3. nullGetCrankAngle (136-136) - Lines=1, N=0, C=1
4. loggerSecondaryISR (332-352) - Lines=21, N=2, C=8
5. loggerTertiaryISR (357-379) - Lines=23, N=2, C=8
6. angleToTimeIntervalTooth (385-400) - Lines=16, N=2, C=2
7. timeToAngleIntervalTooth (403-420) - Lines=18, N=2, C=2
8. engineIsRunning (426-434) - Lines=9, N=2, C=2
9. resetDecoder (436-441) - Lines=6, N=1, C=1
10. UpdateRevolutionTimeFromTeeth (453-464) - Lines=12, N=1, C=6
11. setFilter (484-512) - Lines=29, N=2, C=9
12. __attribute__ (594-606) - Lines=13, N=1, C=3
13. triggerThird_missingTooth (816-846) - Lines=31, N=2, C=5
14. getCrankAngle_missingTooth (866-892) - Lines=27, N=1, C=5
15. triggerSetEndTeeth_missingTooth (897-918) - Lines=22, N=1, C=9
16. triggerRecordVVT1Angle (993-1006) - Lines=14, N=2, C=5
17. getCrankAngle_DualWheel (1131-1160) - Lines=30, N=1, C=6
18. __attribute__ (1162-1170) - Lines=9, N=1, C=1
19. triggerSetEndTeeth_DualWheel (1175-1197) - Lines=23, N=1, C=7
20. getCrankAngle_FordST170 (4906-4932) - Lines=27, N=1, C=5
21. __attribute__ (4934-4943) - Lines=10, N=1, C=1
22. triggerSetEndTeeth_FordST170 (4945-4956) - Lines=12, N=1, C=3
23. triggerSetEndTeeth_NGC (5379-5397) - Lines=19, N=1, C=5
24. triggerSetup_Vmax (5405-5421) - Lines=17, N=1, C=2
25. triggerSec_Vmax (5550-5554) - Lines=5, N=1, C=1
26. getCrankAngle_Vmax (5586-5610) - Lines=25, N=1, C=3
27. triggerSetEndTeeth_Vmax (5612-5614) - Lines=3, N=1, C=1
28. triggerSetup_Renix (5629-5657) - Lines=29, N=2, C=3
29. __attribute__ (5739-5749) - Lines=11, N=1, C=1
30. triggerSetEndTeeth_Renix (5751-5775) - Lines=25, N=1, C=7
31. triggerSetup_RoverMEMS (5795-5817) - Lines=23, N=1, C=2
32. getCrankAngle_RoverMEMS (5959-5985) - Lines=27, N=1, C=5
33. getRPM_RoverMEMS (6069-6086) - Lines=18, N=2, C=6
34. triggerSec_SuzukiK6A (6400-6403) - Lines=4, N=1, C=1
35. getRPM_SuzukiK6A (6405-6415) - Lines=11, N=1, C=2
36. getCrankAngle_SuzukiK6A (6417-6439) - Lines=23, N=2, C=4
37. __attribute__ (6442-6457) - Lines=16, N=2, C=5
38. triggerSetEndTeeth_SuzukiK6A (6459-6464) - Lines=6, N=1, C=1
39. getRPM_FordTFI (6619-6635) - Lines=17, N=2, C=4
40. getCrankAngle_FordTFI (6640-6668) - Lines=29, N=1, C=5
41. triggerSetEndTeeth_FordTFI (6672-6680) - Lines=9, N=1, C=1

---

## 📈 PLANO DE REFATORAÇÃO RECOMENDADO

### Fase 1: CRÍTICA - Imediata
**Objetivo**: Reduzir nesting crítico (N≥5)

Prioridade 1 (Piores casos):
1. `triggerPri_missingTooth` (N=8, C=36, Lines=118)
2. `triggerSec_NGC68` (N=8, C=17, Lines=54)
3. `triggerPri_NGC` (N=7, C=25, Lines=104)

Prioridade 2:
4. `triggerSec_RoverMEMS` (N=6, C=18, Lines=81)
5. `triggerPri_Renix` (N=5, C=17, Lines=73)
6. `triggerSec_NGC4` (N=5, C=17, Lines=49)
7. `triggerSec_FordTFI` (N=5, C=15, Lines=61)

Prioridade 3:
8. `triggerPri_RoverMEMS` (N=5, C=12, Lines=58)
9. `triggerPri_Vmax` (N=5, C=11, Lines=58)
10. `processSimpleSecTrigger` (N=5, C=7, Lines=44)

### Fase 2: ALTA - Urgente
**Objetivo**: Reduzir complexidade ciclomática alta

1. `triggerPri_SuzukiK6A` (C=19, Lines=105)
2. `triggerSetEndTeeth_RoverMEMS` (C=28!, Lines=60) - Complexidade excepcional!

### Fase 3: MÉDIA - Importante
**Objetivo**: Reduzir nesting=4 e complexidade>10

1. `triggerPri_DualWheel` (N=4, C=11)
2. `triggerPri_FordTFI` (N=4, Lines=46)
3. `triggerSec_missingTooth` (N=4)
4. `getRPM_Vmax` (N=4)
5. `checkPerToothTiming` (N=3, C=15)

### Fase 4: BAIXA - Manutenção
**Objetivo**: Reduzir nesting=3 para N≤2

- Refatorar 9 funções com N=3
- Dividir `triggerSetup_SuzukiK6A` (46 linhas)

---

## 🎯 ESTATÍSTICAS DE CONFORMIDADE

### Distribuição de Nesting
| Nesting | Quantidade | Status |
|---------|------------|--------|
| N = 0   | 3 | ✅ Excelente |
| N = 1   | 28 | ✅ Ótimo |
| N = 2   | 10 | ✅ Bom |
| N = 3   | 14 | ⚠️ Aceitável (MISRA: N≤3) |
| N = 4   | 5 | ⚠️ Ruim |
| N = 5   | 6 | 🚨 Crítico |
| N = 6   | 1 | 🚨 Crítico |
| N = 7   | 1 | 🚨 Crítico |
| N = 8   | 2 | 🚨 Crítico |

### Distribuição de Complexidade
| Complexidade | Quantidade | Status |
|--------------|------------|--------|
| C ≤ 5       | 41 | ✅ Excelente |
| C = 6-10    | 15 | ✅ Bom |
| C = 11-15   | 7 | ⚠️ Aceitável (MISRA: C≤10) |
| C = 16-20   | 4 | 🚨 Alto |
| C > 20      | 3 | 🚨 Crítico |

### Distribuição de Tamanho
| Linhas | Quantidade | Status |
|--------|------------|--------|
| < 20   | 29 | ✅ Excelente |
| 20-40  | 21 | ✅ Bom |
| 41-50  | 8 | ⚠️ Limite (MISRA: ≤50) |
| 51-100 | 8 | ⚠️ Grande |
| > 100  | 2 | 🚨 Muito grande |

---

## 📝 OBSERVAÇÕES IMPORTANTES

1. **Progresso da Refatoração**: O arquivo já passou por uma refatoração significativa, com 54.5% do código desabilitado e movido para arquivos separados.

2. **Padrão Identificado**: As violações críticas estão concentradas em funções de trigger primário e secundário de diversos decoders (NGC, RoverMEMS, Renix, Vmax, FordTFI, MissingTooth).

3. **Funções Auxiliares**: As funções auxiliares (getCrankAngle, getRPM, triggerSetEndTeeth) geralmente têm métricas melhores que as funções principais de trigger.

4. **Caso Excepcional**: A função `triggerSetEndTeeth_RoverMEMS` tem complexidade ciclomática de 28, o que é extremamente alto e requer atenção especial.

5. **Conformidade Geral**: 60.3% das funções ativas (41 de 68) já estão em conformidade total com os critérios MISRA estabelecidos.

---

## 🔧 TÉCNICAS DE REFATORAÇÃO RECOMENDADAS

### Para Reduzir Nesting (N)
- **Extract Method**: Extrair blocos internos para funções auxiliares
- **Early Return**: Usar guard clauses para reduzir níveis
- **State Machine**: Converter lógica aninhada em máquina de estados

### Para Reduzir Complexidade (C)
- **Strategy Pattern**: Separar diferentes estratégias em funções/classes
- **Lookup Tables**: Substituir condicionais por tabelas
- **Function Decomposition**: Dividir em funções menores e especializadas

### Para Reduzir Linhas
- **Extract Functions**: Agrupar lógica relacionada
- **Remove Duplication**: Identificar e eliminar código duplicado
- **Simplify Logic**: Revisar e simplificar expressões complexas

---

## 📊 CONCLUSÃO

O arquivo `decoders.cpp` está em processo de refatoração, com mais da metade do código já movido para implementações separadas. Das 68 funções ativas restantes:

- **27 funções (39.7%)** requerem refatoração por violações MISRA
- **41 funções (60.3%)** já estão em conformidade

As 10 funções com violações CRITICAL devem ser priorizadas, especialmente:
1. `triggerPri_missingTooth` (N=8, C=36)
2. `triggerSec_NGC68` (N=8, C=17)
3. `triggerPri_NGC` (N=7, C=25)

A refatoração dessas funções críticas melhorará significativamente a qualidade, manutenibilidade e conformidade MISRA do código.

---

**Relatório gerado em**: 2025-11-05
**Arquivo analisado**: `speeduino/decoders.cpp`
**Total de linhas analisadas**: 6,682
**Funções ativas identificadas**: 68
