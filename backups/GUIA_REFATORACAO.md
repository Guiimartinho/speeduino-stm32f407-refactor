# GUIA DE REFATORAÇÃO - SCG-ECU 2.0

**Objetivo:** Padronizar refatorações futuras com base em lições aprendidas da FASE 1
**Aplicável a:** Todos os arquivos do projeto Speeduino
**Conformidade:** MISRA-C:2012 + REQUISITOS_TECNICOS.md

---

## 📋 CHECKLIST DE REFATORAÇÃO

### Antes de Começar

- [ ] Ler REQUISITOS_TECNICOS.md (padrões obrigatórios)
- [ ] Identificar função alvo (>50 linhas ou C>10)
- [ ] Criar backup do arquivo original
- [ ] Documentar estado atual (linhas, complexidade, nesting)
- [ ] Planejar estratégia de refatoração

### Durante a Refatoração

- [ ] Aplicar padrão apropriado (ver seção "Padrões")
- [ ] Manter funções < 50 linhas
- [ ] Manter complexidade < 10
- [ ] Manter nesting ≤ 3 níveis
- [ ] Aplicar guard clauses
- [ ] Adicionar documentação Doxygen
- [ ] Preservar funcionalidade (não mudar comportamento)

### Após Refatoração

- [ ] Build limpo (0 errors, 0 warnings)
- [ ] Validar métricas (linhas, C, N)
- [ ] Testar funcionalidade (se possível)
- [ ] Commitar com mensagem detalhada
- [ ] Atualizar documentação (VALIDACAO_CODIGO_REAL.md)

---

## 🎯 PADRÕES DE REFATORAÇÃO

### Padrão 1: Command Handler Pattern

**Quando Usar:**
- Switch statements grandes (>5 cases)
- Cada case tem lógica substancial (>10 linhas)
- Casos são independentes entre si

**Exemplo:** legacySerialCommand() (470 linhas → 48 linhas)

**ANTES:**
```cpp
void legacySerialCommand(void) {
  switch (currentCommand) {
    case 'A':
      // 15 linhas de lógica inline
      sendValues(0, LOG_ENTRY_SIZE, 0x31, primarySerial, serialStatusFlag);
      firstCommsRequest = false;
      break;

    case 'B':
      // 10 linhas de lógica inline
      BIT_SET(currentStatus.status4, BIT_STATUS4_COMMS_COMPAT);
      legacySerialHandler(currentCommand, Serial, serialStatusFlag);
      break;

    // ... 36 more cases
  }
}
```

**DEPOIS:**
```cpp
// Anonymous namespace com handlers
namespace {

/** @brief Handler for 'A' command - Send realtime values
 *  @complexity Low (C=1, N=1)
 */
static void handleCommand_A(void) {
  sendValues(0, LOG_ENTRY_SIZE, 0x31, primarySerial, serialStatusFlag);
  firstCommsRequest = false;
}

/** @brief Handler for 'B' command - Burn with compat mode
 *  @complexity Low (C=1, N=1)
 */
static void handleCommand_B(void) {
  BIT_SET(currentStatus.status4, BIT_STATUS4_COMMS_COMPAT);
  legacySerialHandler(currentCommand, Serial, serialStatusFlag);
}

// ... 36 more handlers

} // anonymous namespace

/** @brief Command dispatcher - Reduced from 470 to 48 lines
 *  @complexity Low (C=38 for switch, O(1) per branch)
 */
void legacySerialCommand(void) {
  serialReceiveStartTime = millis();
  if (serialStatusFlag == SERIAL_INACTIVE) {
    currentCommand = primarySerial.read();
  }

  switch (currentCommand) {
    case 'A': handleCommand_A(); break;
    case 'B': handleCommand_B(); break;
    // ... 36 more cases
    default: serialStatusFlag = SERIAL_INACTIVE; break;
  }
}
```

**Checklist:**
- [ ] Criar anonymous namespace
- [ ] Extrair cada case para função static
- [ ] Nomear handlers: `handleCommand_X()` ou `handle<Action>()`
- [ ] Dispatcher: apenas switch + chamadas de função
- [ ] Documentar cada handler com Doxygen
- [ ] Incluir @complexity tag

**Benefícios:**
- ✅ Redução: 90% típico
- ✅ Testabilidade: Cada handler isolado
- ✅ Manutenibilidade: Modificar 1 comando não afeta outros
- ✅ Legibilidade: Dispatcher cabe em 1 tela

---

### Padrão 2: Helper Extraction

**Quando Usar:**
- Código duplicado em múltiplas funções
- Lógica complexa que obscurece função principal
- Seção de código com responsabilidade bem definida

**Exemplo:** sendToothLog() & sendCompositeLog()

**ANTES (Código Duplicado):**
```cpp
void sendToothLog(void) {
  // Inicialização de pacote (10 linhas)
  (void)serialWrite((uint16_t)(sizeof(toothHistory) + 1U));
  const uint8_t returnCode = SERIAL_RC_OK;
  CRC32_val = CRC32_serial.crc32(&returnCode, 1, false);
  writeByteReliableBlocking(returnCode);

  // Lógica de transmissão...

  // Finalização CRC (5 linhas)
  CRC32_val = ~CRC32_val;
  (void)serialWrite(CRC32_val);
}

void sendCompositeLog(void) {
  // CÓDIGO DUPLICADO - Inicialização de pacote (10 linhas)
  (void)serialWrite((uint16_t)(sizeof(toothHistory) + sizeof(compositeLogHistory) + 1U));
  const uint8_t returnCode = SERIAL_RC_OK;
  CRC32_val = CRC32_serial.crc32(&returnCode, 1, false);
  writeByteReliableBlocking(returnCode);

  // Lógica de transmissão...

  // CÓDIGO DUPLICADO - Finalização CRC (5 linhas)
  CRC32_val = ~CRC32_val;
  (void)serialWrite(CRC32_val);
}
```

**DEPOIS (Helpers Compartilhados):**
```cpp
namespace {

/** @brief Initialize packet transmission and CRC for log data
 *  @param packetSize Total size of packet payload in bytes
 *  @return CRC32 value initialized with return code
 *  @complexity Low (C=1, N=1)
 */
static uint32_t initializeLogPacket(uint16_t packetSize) {
  (void)serialWrite((uint16_t)(packetSize + 1U));
  const uint8_t returnCode = SERIAL_RC_OK;
  uint32_t CRC32_val = CRC32_serial.crc32(&returnCode, 1, false);
  writeByteReliableBlocking(returnCode);
  return CRC32_val;
}

/** @brief Finalize and transmit CRC32 checksum
 *  @param CRC32_val Accumulated CRC value to finalize and send
 *  @complexity Low (C=1, N=1)
 */
static void finalizeLogPacket(uint32_t CRC32_val) {
  CRC32_val = ~CRC32_val;
  (void)serialWrite(CRC32_val);
}

} // anonymous namespace

void sendToothLog(void) {
  // Usa helper compartilhado
  uint32_t CRC32_val = initializeLogPacket(sizeof(toothHistory));

  // Lógica de transmissão...

  // Usa helper compartilhado
  finalizeLogPacket(CRC32_val);
}

void sendCompositeLog(void) {
  // Usa MESMO helper
  uint32_t CRC32_val = initializeLogPacket(sizeof(toothHistory) + sizeof(compositeLogHistory));

  // Lógica de transmissão...

  // Usa MESMO helper
  finalizeLogPacket(CRC32_val);
}
```

**Checklist:**
- [ ] Identificar código duplicado (copiar/colar)
- [ ] Extrair para função helper em anonymous namespace
- [ ] Nomear descritivamente: `initialize...()`, `validate...()`, `calculate...()`
- [ ] Parametrizar diferenças (tamanho, tipo, etc.)
- [ ] Substituir código original por chamada ao helper
- [ ] Documentar com Doxygen

**Benefícios:**
- ✅ DRY (Don't Repeat Yourself)
- ✅ Reduz erros de copy-paste
- ✅ Centraliza lógica (1 lugar para modificar)
- ✅ Facilita testes

---

### Padrão 3: Guard Clauses (Early Returns)

**Quando Usar:**
- Deep nesting (>3 níveis)
- Múltiplas condições de erro/validação
- Lógica principal obscurecida por checks

**Exemplo:** handleCommand_g()

**ANTES (Deep Nesting):**
```cpp
static void handleCommand_g(void) {
  if (primarySerial.available() >= 3) {                    // N=1
    uint16_t eepromSize = word(primarySerial.read(), primarySerial.read());

    if (eepromSize == getEEPROMSize()) {                   // N=2
      for (uint16_t x = 0; x < eepromSize; x++) {          // N=3
        if (primarySerial.available()) {                   // N=4
          EEPROMWriteRaw(x, primarySerial.read());
        } else {                                           // N=4
          // Timeout
          serialStatusFlag = SERIAL_INACTIVE;
          break;
        }
      }
    } else {                                               // N=2
      primarySerial.println(F("ERR; Incorrect EEPROM size"));
    }

    serialStatusFlag = SERIAL_INACTIVE;
  }
}
```

**DEPOIS (Guard Clauses):**
```cpp
static void handleCommand_g(void) {
  serialStatusFlag = SERIAL_COMMAND_INPROGRESS_LEGACY;

  // Guard 1: Insuficient data
  while ((primarySerial.available() < 3) && (!isRxTimeout())) { delay(1); }
  if (primarySerial.available() < 3) {
    serialStatusFlag = SERIAL_INACTIVE;
    return;
  }

  // Guard 2: Wrong EEPROM size
  uint16_t eepromSize = word(primarySerial.read(), primarySerial.read());
  if (eepromSize != getEEPROMSize()) {
    primarySerial.println(F("ERR; Incorrect EEPROM size"));
    serialStatusFlag = SERIAL_INACTIVE;
    return;
  }

  // Happy path (low nesting)
  for (uint16_t x = 0; x < eepromSize; x++) {              // N=1
    while ((primarySerial.available() == 0) && (!isRxTimeout())) { delay(1); }

    if (primarySerial.available()) {                       // N=2
      EEPROMWriteRaw(x, primarySerial.read());
    } else {                                               // N=2
      // Timeout
      serialStatusFlag = SERIAL_INACTIVE;
      return;
    }
  }

  serialStatusFlag = SERIAL_INACTIVE;
}
```

**Checklist:**
- [ ] Identificar condições de erro/validação
- [ ] Mover para início da função (guard clauses)
- [ ] Usar early return (não else)
- [ ] "Happy path" fica por último (baixo nesting)
- [ ] Comentar cada guard clause

**Benefícios:**
- ✅ Nesting: 4 → 2 níveis (típico)
- ✅ Legibilidade: Casos de erro claros
- ✅ Manutenibilidade: Fácil adicionar validações
- ✅ Performance: Early exit

---

### Padrão 4: Sub-handlers para Lógica Complexa

**Quando Usar:**
- Um handler Command tem >50 linhas
- Handler tem múltiplas responsabilidades
- Deep nesting dentro do handler

**Exemplo:** handleCommand_r() - SD Card operations

**ANTES (Handler Monolítico):**
```cpp
static void handleCommand_r(void) {
  uint8_t cmd = serialPayload[2];
  uint16_t offset = word(serialPayload[4], serialPayload[3]);
  uint16_t length = word(serialPayload[6], serialPayload[5]);
  uint16_t SD_arg1 = word(serialPayload[3], serialPayload[4]);
  uint16_t SD_arg2 = word(serialPayload[5], serialPayload[6]);

  if (cmd == SEND_OUTPUT_CHANNELS) {
    generateLiveValues(offset, length);
    sendSerialPayloadNonBlocking(length + 1U);
  } else if (cmd == SD_RTC_PAGE) {
    // 15 linhas de lógica RTC
    serialPayload[0] = SERIAL_RC_OK;
    serialPayload[1] = rtc_getSecond();
    // ... 13 more lines
  } else if (cmd == SD_READWRITE_PAGE) {
    if ((SD_arg1 == SD_READ_STAT_ARG1) && (SD_arg2 == SD_READ_STAT_ARG2)) {
      // 35 linhas de lógica de status
      serialPayload[0] = SERIAL_RC_OK;
      // ... 33 more lines
    } else if ((SD_arg1 == SD_READ_DIR_ARG1) && (SD_arg2 == SD_READ_DIR_ARG2)) {
      // 18 linhas de lógica de diretório
      // ...
    }
  } else if (cmd == SD_READFILE_PAGE) {
    // 28 linhas de lógica de leitura de arquivo
    // ...
  }
}
```

**DEPOIS (Sub-handlers):**
```cpp
namespace {

/** @brief Sub-handler: Read SD card RTC values */
static void handleCommand_r_ReadRTC(void) {
  serialPayload[0] = SERIAL_RC_OK;
  serialPayload[1] = rtc_getSecond();
  serialPayload[2] = rtc_getMinute();
  // ... remaining fields
  sendSerialPayloadNonBlocking(9);
}

/** @brief Sub-handler: Read SD card status */
static void handleCommand_r_ReadSDStatus(void) {
  serialPayload[0] = SERIAL_RC_OK;
  serialPayload[1] = currentStatus.TS_SD_Status;
  // ... remaining fields
  sendSerialPayloadNonBlocking(17);
}

/** @brief Sub-handler: Read SD directory listing */
static void handleCommand_r_ReadDirectory(void) {
  serialPayload[0] = SERIAL_RC_OK;
  // ... logic
  sendSerialPayloadNonBlocking(payloadIndex + 2);
}

/** @brief Sub-handler: Read file data from SD card */
static void handleCommand_r_ReadFileData(uint16_t SD_arg1) {
  serialPayload[0] = SERIAL_RC_OK;
  // ... logic
}

} // anonymous namespace

/** @brief Handler for 'r' command - Optimized output channels
 *  @complexity Medium (C=6, N=3) - decomposed from C=12, N=5
 *  @note Original 133 lines reduced to 46 lines via sub-handlers
 */
static void handleCommand_r(void) {
  uint8_t cmd = serialPayload[2];
  uint16_t offset = word(serialPayload[4], serialPayload[3]);
  uint16_t length = word(serialPayload[6], serialPayload[5]);

  // Primary command
  if (cmd == SEND_OUTPUT_CHANNELS) {
    generateLiveValues(offset, length);
    sendSerialPayloadNonBlocking(length + 1U);
    return;
  }

#ifdef COMMS_SD
  uint16_t SD_arg1 = word(serialPayload[3], serialPayload[4]);
  uint16_t SD_arg2 = word(serialPayload[5], serialPayload[6]);

  // Dispatch to appropriate sub-handler
  if (cmd == SD_RTC_PAGE) {
    handleCommand_r_ReadRTC();
  } else if (cmd == SD_READWRITE_PAGE) {
    if ((SD_arg1 == SD_READ_STAT_ARG1) && (SD_arg2 == SD_READ_STAT_ARG2)) {
      handleCommand_r_ReadSDStatus();
    } else if ((SD_arg1 == SD_READ_DIR_ARG1) && (SD_arg2 == SD_READ_DIR_ARG2)) {
      handleCommand_r_ReadDirectory();
    }
  } else if (cmd == SD_READFILE_PAGE) {
    handleCommand_r_ReadFileData(SD_arg1);
  }
#endif
}
```

**Checklist:**
- [ ] Identificar sub-responsabilidades dentro do handler
- [ ] Extrair cada uma para sub-handler dedicado
- [ ] Nomear: `handleCommand_X_SubAction()`
- [ ] Handler principal vira dispatcher de sub-handlers
- [ ] Documentar cada sub-handler

**Benefícios:**
- ✅ Handler principal < 50 linhas
- ✅ Cada sub-handler tem 1 responsabilidade
- ✅ Facilita testes isolados
- ✅ Reduz complexidade e nesting

---

## 📐 MÉTRICAS E VALIDAÇÃO

### Como Medir Conformidade

**Linhas de Código:**
```bash
# Contar linhas de uma função
sed -n '/^void myFunction/,/^}/p' file.cpp | wc -l
```

**Complexidade Ciclomática (Estimativa Manual):**
```
C = 1 + (if) + (else if) + (for) + (while) + (case) + (&&) + (||)
```

**Exemplo:**
```cpp
void example() {              // C = 1 (base)
  if (a) {                    // C = 2 (+1)
    for (int i = 0; i < 10; i++) {  // C = 3 (+1)
      if (b && c) {           // C = 5 (+1 if, +1 &&)
        /* ... */
      }
    }
  } else if (d) {             // C = 6 (+1)
    /* ... */
  }
}
// Total: C = 6
```

**Nesting Level (Manual):**
```cpp
void example() {
  if (a) {                    // N = 1
    for (int i = 0; i < 10; i++) {    // N = 2
      if (b) {                // N = 3
        while (c) {           // N = 4 ← EXCEDE LIMITE!
```

### Ferramentas Automatizadas

**Lizard (Complexidade):**
```bash
pip install lizard
lizard file.cpp -l cpp
```

**Cppcheck (Análise Estática):**
```bash
cppcheck --enable=all --suppress=missingInclude file.cpp
```

**Clang-tidy (MISRA-C):**
```bash
clang-tidy file.cpp -checks='readability-*,modernize-*,misra-*'
```

---

## 🔧 TEMPLATE DE REFATORAÇÃO

### Template 1: Command Handler

```cpp
// ============================================================================
// COMMAND HANDLERS - Anonymous Namespace (MISRA-C:2012 Compliance)
// ============================================================================
// Following REQUISITOS_TECNICOS.md:
// - Each handler < 50 lines
// - Complexity < 10 per function
// - Nesting ≤ 3 levels
// - Guard clauses for early returns
// - Single Responsibility Principle
// ============================================================================

namespace {

/** @brief Handler for 'X' command - Description
 *  @complexity Low/Medium/High (C=X, N=Y)
 *  @param [if applicable]
 *  @return [if applicable]
 *  @note [special considerations]
 */
static void handleCommand_X(void) {
  // Guard clauses first
  if (errorCondition) {
    cleanup();
    return;
  }

  // Happy path
  // ... implementation (< 50 lines)
}

// ... more handlers

} // anonymous namespace

/** @brief Dispatcher - Reduced from XXX to YY lines
 *
 * Following REQUISITOS_TECNICOS.md Command Handler pattern:
 * - Complexity: N cases (one per command) - acceptable for pure dispatch
 * - Nesting: 1 level only (switch statement)
 * - Each case delegates to dedicated handler function
 * - Zero business logic in dispatcher
 *
 * @complexity Low (C=N for switch, but each branch is O(1))
 * @performance O(1) dispatch via switch jump table
 * @note This function went from XXX lines to YY lines (ZZ% reduction)
 */
void dispatcherFunction(void) {
  // Minimal setup

  switch (command) {
    case 'X': handleCommand_X(); break;
    // ... more cases
    default: handleError(); break;
  }
}
```

### Template 2: Helper Function

```cpp
/** @brief Helper description - What it does
 *  @param param1 Description of param1
 *  @param param2 Description of param2
 *  @return Description of return value
 *  @complexity Low/Medium/High (C=X, N=Y)
 *  @note [Special considerations, side effects, etc.]
 */
static ReturnType helperFunction(Type1 param1, Type2 param2) {
  // Guard clauses
  if (invalidInput) {
    return errorValue;
  }

  // Implementation
  // ... (< 50 lines)

  return result;
}
```

---

## ⚠️ ARMADILHAS COMUNS

### 1. Extração Automática Incompleta

**Problema:** Scripts podem cortar funções no meio

**Prevenção:**
```bash
# Sempre validar sintaxe após extração
gcc -fsyntax-only file.cpp

# Ou usar build completo
pio run
```

### 2. Ordem de Declaração

**Problema:** Função chama helper antes de declarar

**Solução:**
```cpp
// ERRADO
void publicFunction() {
  helperFunction(); // ERROR: not declared
}

namespace {
  static void helperFunction() { /* ... */ }
}

// CORRETO
namespace {
  static void helperFunction() { /* ... */ }
} // anonymous namespace

void publicFunction() {
  helperFunction(); // OK
}
```

### 3. Over-engineering

**Problema:** Criar muitos níveis de abstração

**Exemplo RUIM:**
```cpp
// Abstração excessiva para código trivial
static void setStatusInactive(void) {
  serialStatusFlag = SERIAL_INACTIVE;
}

static void handleCommand_Simple(void) {
  doSomething();
  setStatusInactive(); // Overkill para 1 linha
}
```

**Exemplo BOM:**
```cpp
static void handleCommand_Simple(void) {
  doSomething();
  serialStatusFlag = SERIAL_INACTIVE; // Direto, claro
}
```

**Regra:** Só extrair helpers se:
- Código duplicado (≥2 usos)
- Lógica complexa (≥5 linhas)
- Responsabilidade bem definida

---

## 📝 TEMPLATE DE COMMIT

```
<type>: <subject> - MISRA-C compliance

<detailed description>

CHANGES SUMMARY:
================

1. <function_name>() - <before> → <after> lines
   - <specific changes>
   - Created <N> handlers/helpers

2. <function_name>() - <before> → <after> lines
   - <specific changes>

METRICS ACHIEVED:
- ✅ All functions < 50 lines (max: XX lines)
- ✅ Complexity < 10 (max: C=X)
- ✅ Nesting ≤ 3 (max: N=Y)
- ✅ Guard clauses throughout
- ✅ Single Responsibility Principle
- ✅ Doxygen documentation complete

BUILD STATUS:
✅ Compilation: SUCCESS
✅ Warnings: 0
✅ Errors: 0

CO-LOCATED FILES:
- <file>.cpp.backup_pre_refactor (original preserved)
```

---

## 🎯 PRÓXIMOS PASSOS

### Para Refatorar init.cpp

1. **configureCylinderTimings()** (515 linhas):
   - Aplicar Command Handler Pattern
   - 8 handlers (cylinders 1, 2, 3, 4, 5, 6, 8, default)
   - Estimativa: 515 → 60 linhas

2. **configureIgnitionMode()** (263 linhas):
   - Aplicar decomposição funcional
   - Extrair helpers para cada modo
   - Estimativa: 263 → 150 linhas

3. **initialiseAll()** (217 linhas):
   - Aplicar decomposição sequencial
   - Extrair seções lógicas
   - Estimativa: 217 → 120 linhas

---

## 📚 REFERÊNCIAS

**Documentos do Projeto:**
- REQUISITOS_TECNICOS.md - Padrões obrigatórios (NÃO MODIFICAR)
- VALIDACAO_CODIGO_REAL.md - Status atual
- RELATORIO_FASE1.md - Lições aprendidas

**Padrões de Projeto:**
- Command Pattern (GoF)
- Extract Method (Fowler)
- Guard Clauses (Beck)
- SOLID Principles

**MISRA-C Guidelines:**
- MISRA-C:2012 - Rule 15.5 (Function length)
- MISRA-C:2012 - Rule 15.1 (Complexity)
- MISRA-C:2012 - Rule 15.6 (Nesting)

---

**Última Atualização:** 04/11/2025
**Próxima Revisão:** Após conclusão de init.cpp
**Versão:** 1.0
