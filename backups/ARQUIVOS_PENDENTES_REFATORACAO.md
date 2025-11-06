# ARQUIVOS PENDENTES DE REFATORAÇÃO - SCG-ECU 2.0

**Data:** 02/11/2025
**Projeto:** SCG-ECU 2.0 - STM32F407VGT6 8x8
**Status:** 7/7 módulos principais completos, 37 arquivos auxiliares pendentes

---

## RESUMO EXECUTIVO

### Status Atual

```
Módulos Completos (7/7):
✅ Board Configuration
✅ Auxiliaries
✅ Decoders
✅ Corrections
✅ Sensors
✅ Table Access
✅ Schedulers

Arquivos Auxiliares Pendentes: 37
  - Alta Prioridade: 1 arquivo (comms_legacy.cpp)
  - Média Prioridade: 4 arquivos
  - Baixa Prioridade: 32 arquivos
```

### Arquivos de Backup

**Ação Concluída:** Todos os 38 arquivos de backup foram movidos para:
```
/home/guiito/Documents/1.Projects/speeduino_backup/
```

Estrutura de diretórios preservada para referência futura.

---

## PRIORIZAÇÃO PARA REFATORAÇÃO

### 🔴 PRIORIDADE ALTA (1 arquivo)

#### 1. comms_legacy.cpp
- **Linhas:** 1,305
- **Estruturas de controle:** ~85
- **Funções:** ~8
- **Score de Complexidade:** 21
- **Categoria:** Comunicação Serial Legacy
- **Motivo:** Arquivo muito grande com alta complexidade ciclomática
- **Recomendação:** Dividir em módulos menores por protocolo

---

### 🟡 PRIORIDADE MÉDIA (4 arquivos)

#### 2. updates.cpp
- **Linhas:** 860
- **Estruturas de controle:** ~76
- **Funções:** ~5
- **Score:** 15
- **Categoria:** Atualização de Firmware
- **Recomendação:** Extrair lógica de atualização por tipo

#### 3. SD_logger.cpp
- **Linhas:** 811
- **Estruturas de controle:** ~59
- **Funções:** ~16
- **Score:** 13
- **Categoria:** Data Logging SD
- **Recomendação:** Modularizar por tipo de log

#### 4. comms_CAN.cpp
- **Linhas:** 808
- **Estruturas de controle:** ~33
- **Funções:** ~9
- **Score:** 11
- **Categoria:** Comunicação CAN
- **Recomendação:** Separar CAN secundário

#### 5. speeduino.cpp (Loop Principal)
- **Linhas:** 344
- **Estruturas de controle:** ~21
- **Funções:** ~2
- **Score:** 5
- **Categoria:** Loop Principal
- **Recomendação:** Simplificar loop() extraindo subsistemas

---

### 🟢 PRIORIDADE BAIXA (32 arquivos)

Arquivos menores com complexidade moderada que podem ser refatorados após os prioritários.

---

## ARQUIVOS POR CATEGORIA

### Comunicação (3 arquivos - ALTA/MÉDIA prioridade)

| Arquivo | Linhas | Prioridade | Nota |
|---------|--------|------------|------|
| comms_legacy.cpp | 1,305 | 🔴 ALTA | Protocolo serial legacy |
| comms_CAN.cpp | 808 | 🟡 MÉDIA | Comunicação CAN principal |
| comms_secondary.cpp | 205 | 🟢 BAIXA | CAN secundário |

**Estratégia Recomendada:**
1. Começar por comms_legacy.cpp (mais crítico)
2. Aplicar pattern modularização usado em corrections.cpp
3. Extrair cada comando serial em função dedicada
4. Implementar state machine para protocolo

---

### Data Logging (2 arquivos - MÉDIA prioridade)

| Arquivo | Linhas | Prioridade | Nota |
|---------|--------|------------|------|
| SD_logger.cpp | 811 | 🟡 MÉDIA | Logging em SD card |
| logger.cpp | 653 | 🟢 BAIXA | Logger principal |

**Estratégia Recomendada:**
1. Separar lógica de formatação de I/O
2. Criar módulo buffer circular
3. Implementar async logging

---

### Armazenamento (2 arquivos - BAIXA prioridade)

| Arquivo | Linhas | Prioridade | Nota |
|---------|--------|------------|------|
| storage.cpp | 627 | 🟢 BAIXA | EEPROM/Flash storage |
| pages.cpp | 484 | 🟢 BAIXA | Gestão páginas config |

---

### Atualização Firmware (1 arquivo - MÉDIA prioridade)

| Arquivo | Linhas | Prioridade | Nota |
|---------|--------|------------|------|
| updates.cpp | 860 | 🟡 MÉDIA | Lógica de update FW |

**Estratégia Recomendada:**
1. Extrair validação CRC
2. Separar flash programming
3. Implementar rollback safety

---

### Cálculos (2 arquivos - BAIXA prioridade)

| Arquivo | Linhas | Prioridade | Nota |
|---------|--------|------------|------|
| fuel_calculations.cpp | 414 | 🟢 BAIXA | Cálculos combustível |
| ignition_calculations.cpp | 218 | 🟢 BAIXA | Cálculos ignição |

---

### Sensores (1 arquivo - BAIXA prioridade)

| Arquivo | Linhas | Prioridade | Nota |
|---------|--------|------------|------|
| sensor_polling.cpp | 435 | 🟢 BAIXA | Polling de sensores |

---

### Proteção Motor (2 arquivos - BAIXA prioridade)

| Arquivo | Linhas | Prioridade | Nota |
|---------|--------|------------|------|
| engine_protection.cpp | 345 | 🟢 BAIXA | Proteções motor |
| engineProtection.cpp | 228 | 🟢 BAIXA | Variante alternativa |

**Nota:** Verificar duplicação entre engine_protection.cpp e engineProtection.cpp

---

### Utilitários (4 arquivos - BAIXA prioridade)

| Arquivo | Linhas | Prioridade | Nota |
|---------|--------|------------|------|
| timers.cpp | 384 | 🟢 BAIXA | Temporização |
| TS_CommandButtonHandler.cpp | 393 | 🟢 BAIXA | Handler botões TS |
| utilities.cpp | 277 | 🟢 BAIXA | Funções utilitárias |
| rtc_common.cpp | 129 | 🟢 BAIXA | RTC common |

---

### Outros (15 arquivos - BAIXA prioridade)

| Arquivo | Linhas | Tipo | Nota |
|---------|--------|------|------|
| globals.cpp | 287 | Global | Variáveis globais |
| scheduledIO.cpp | 132 | I/O | Scheduled I/O |
| secondaryTables.cpp | 181 | Tabelas | Tabelas secundárias |
| config_pages.h | 1,003 | Header | Estruturas config |
| bit_shifts.h | 686 | Header | Bit manipulation |
| maths.h | 399 | Header | Funções matemáticas |
| globals.h | 360 | Header | Declarações globais |
| scheduledIO.h | 274 | Header | I/O scheduling |
| automotive_constants.h | 272 | Header | Constantes automotivas |
| statuses.h | 250 | Header | Status structs |
| table3d_values.h | 202 | Header | Valores 3D |
| storage.h | 200 | Header | Storage API |
| auxiliaries.h | 172 | Header | Auxiliares |
| modularization_globals.h | 119 | Header | Globals modular |
| table3d_axes.h | 112 | Header | Eixos 3D |

---

## ROADMAP DE REFATORAÇÃO SUGERIDO

### FASE 8: Comunicação (4-6 semanas)

**Objetivo:** Refatorar subsistema de comunicação

**Arquivos:**
1. comms_legacy.cpp (1ª prioridade)
2. comms_CAN.cpp
3. comms_secondary.cpp
4. logger.cpp

**Padrões a Aplicar:**
- State machine para protocolo serial
- Command pattern para comandos
- Buffer circular para comunicação
- Guard clauses obrigatórias
- Funções < 50 linhas

**Entregáveis:**
- Módulos communication/ separados
- Testes unitários comunicação
- Documentação protocolo

---

### FASE 9: Data Logging (2-3 semanas)

**Objetivo:** Modularizar sistema de logging

**Arquivos:**
1. SD_logger.cpp
2. logger.cpp (se não feito na Fase 8)

**Padrões a Aplicar:**
- Async logging com buffer
- Separação formatação/I/O
- Error handling robusto

---

### FASE 10: Armazenamento & Updates (2-3 semanas)

**Objetivo:** Refatorar persistência e atualização

**Arquivos:**
1. updates.cpp
2. storage.cpp
3. pages.cpp

**Padrões a Aplicar:**
- CRC validation
- Rollback safety
- Flash programming isolado

---

### FASE 11: Loop Principal & Proteções (2 semanas)

**Objetivo:** Simplificar loop principal

**Arquivos:**
1. speeduino.cpp
2. engine_protection.cpp
3. engineProtection.cpp (consolidar)

**Padrões a Aplicar:**
- Extração subsistemas
- State machine principal
- Proteções centralizadas

---

### FASE 12: Cálculos & Sensores (1-2 semanas)

**Objetivo:** Refatorar cálculos auxiliares

**Arquivos:**
1. fuel_calculations.cpp
2. ignition_calculations.cpp
3. sensor_polling.cpp

**Padrões a Aplicar:**
- Lookup tables otimizadas
- Inline functions onde apropriado
- Comentários algoritmos

---

### FASE 13: Utilitários & Cleanup (1-2 semanas)

**Objetivo:** Finalizar arquivos menores

**Arquivos:**
- timers.cpp
- utilities.cpp
- TS_CommandButtonHandler.cpp
- scheduledIO.cpp
- Todos os headers restantes

**Padrões a Aplicar:**
- Consolidação funções similares
- Namespace organization
- Documentação completa

---

## PADRÕES DE REFATORAÇÃO A SEGUIR

Baseado em `docs/REQUISITOS_TECNICOS.md`:

### 1. Estrutura de Código

```cpp
// ✅ BOM - Guard clauses
ReturnType functionName(params) {
    // Early returns para casos especiais
    if (invalidCondition) { return errorValue; }
    if (edgeCase) { return specialValue; }

    // Lógica principal no caminho feliz
    // ...
    return result;
}

// ❌ RUIM - Aninhamento profundo
ReturnType functionName(params) {
    if (validCondition) {
        if (anotherCheck) {
            if (yetAnother) {
                // Lógica enterrada 3+ níveis
            }
        }
    }
}
```

### 2. Complexidade

- **Funções:** < 50 linhas (exceto lookup tables)
- **Complexidade Ciclomática:** < 10
- **Aninhamento:** ≤ 3 níveis
- **Parâmetros:** ≤ 5 por função

### 3. ISRs (Interrupt Service Routines)

- Overhead máximo: 10µs
- Sem chamadas complexas
- Flags + processamento no loop principal
- Inline quando possível

### 4. Nomenclatura

```cpp
// Funções: camelCase
void calculateFuelPulseWidth();

// Variáveis: camelCase
uint16_t engineRPM;

// Constantes: UPPER_CASE
#define MAX_RPM_LIMIT 8000

// Tipos: PascalCase
struct SensorData { ... };
```

### 5. Documentação

```cpp
/**
 * @brief Calcula largura do pulso de injeção
 * @param MAP Pressão do coletor (kPa)
 * @param RPM Rotação do motor (RPM)
 * @return Pulso em microsegundos
 */
uint16_t calculateInjectorPulse(uint16_t MAP, uint16_t RPM);
```

---

## MÉTRICAS DE SUCESSO

### Por Arquivo Refatorado

- [ ] Todas as funções < 50 linhas
- [ ] Complexidade ciclomática < 10
- [ ] Aninhamento ≤ 3 níveis
- [ ] Guard clauses implementadas
- [ ] Comentários atualizados
- [ ] Testes unitários criados
- [ ] Build passa sem warnings
- [ ] Performance mantida (ISRs)

### Por Fase

- [ ] Documentação atualizada
- [ ] Backup original criado
- [ ] Diff logic verification
- [ ] RAM/Flash não aumentados
- [ ] HIL tests passam (se aplicável)

---

## FERRAMENTAS DE ANÁLISE

### Análise de Complexidade

```bash
# Instalar lizard (complexity analyzer)
pip install lizard

# Analisar arquivo
lizard speeduino/comms_legacy.cpp

# Analisar projeto completo
lizard speeduino/*.cpp -l cpp -C 10 -L 50
```

### Análise Estática

```bash
# Cppcheck
cppcheck --enable=all --std=c++11 speeduino/

# Clang-tidy
clang-tidy speeduino/*.cpp -checks='*'
```

---

## PRÓXIMOS PASSOS

1. ✅ **Backups movidos** para `/home/guiito/Documents/1.Projects/speeduino_backup/`
2. 📋 **Este documento criado** com análise completa
3. ⏭️ **Iniciar Fase 8** com comms_legacy.cpp
4. 📊 **Criar branch** `refactor/phase8-communication`
5. 🔨 **Aplicar padrões** módulo por módulo

---

## REFERÊNCIAS

- `docs/REQUISITOS_TECNICOS.md` - Padrões de código
- `docs/PROJETO_SCG_ECU_MASTER_REFERENCE.md` - Arquitetura
- `docs/IMPLEMENTACAO_MODULARIZACAO_STATUS.md` - Módulos completos
- `docs/DECODERS_REFACTOR_COMPLETE_REPORT.md` - Exemplo refatoração

---

**FIM DO RELATÓRIO**

**Versão:** 1.0
**Data:** 02/11/2025
**Autor:** Análise Automática + Revisão Manual
