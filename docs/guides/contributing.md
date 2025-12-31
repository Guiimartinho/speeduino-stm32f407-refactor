# Guia de Contribuio - SCG-ECU 2.0

## Modularizao e Adaptao Speeduino para STM32F407VGT6

**Verso:** 2.0
**ltima Atualizao:** 2025-12-30

---

## Sobre Este Projeto

O **SCG-ECU 2.0**  uma modularizao e adaptao completa do firmware Speeduino original para a plataforma STM32F407VGT6 com configurao 8x8 (8 injetores + 8 ignies).

**Projeto Base:** Speeduino (https://speeduino.com) por Josh Stewart

### Diferenciais Deste Fork

| Aspecto | Speeduino Original | SCG-ECU 2.0 |
|---------|-------------------|-------------|
| Arquitetura | Monoltica | **Modular (Interface+Registry+Coordinator)** |
| Compliance | Informal | **MISRA-C:2012 (97%)** |
| Testes | Mnimos | **313 testes unitrios** |
| ISR Performance | Baseline | **+20-30% mais rpido** |
| Plataforma | Multi-target | **STM32F407VGT6 exclusivo** |

---

## Antes de Contribuir

### Leitura Obrigatria

1. **[../reference/02_REQUISITOS_TECNICOS.md](../reference/02_REQUISITOS_TECNICOS.md)** - Padres de cdigo OBRIGATRIOS
2. **[GIT_COMMIT_RULES_MANDATORY.md](GIT_COMMIT_RULES_MANDATORY.md)** - Regras de commit
3. **[../reference/01_PROJETO_SCG_ECU_MASTER_REFERENCE.md](../reference/01_PROJETO_SCG_ECU_MASTER_REFERENCE.md)** - Referncia master

### Recursos da Comunidade

- **Speeduino Discord:** https://speeduino.com/home/community/discord
- **Speeduino Wiki:** https://wiki.speeduino.com
- **Speeduino Forum:** https://speeduino.com/forum

---

## Padres de Cdigo OBRIGATRIOS

### Complexidade e Estrutura

| Mtrica | Limite | Target |
|--------|--------|--------|
| Complexidade Ciclomtica | < 10 | < 7 |
| Aninhamento Mximo | 3 nveis | 2 nveis |
| Tamanho de Funo | < 50 linhas | 20-30 linhas |
| ISR Performance | < 10s | < 5s |

### Guard Clauses (OBRIGATRIO)

```cpp
// ERRADO - aninhamento profundo
void processEngine() {
  if (condition1) {
    if (condition2) {
      if (condition3) {
        // lgica
      }
    }
  }
}

// CORRETO - guard clauses
void processEngine() {
  if (!condition1) { return; }
  if (!condition2) { return; }
  if (!condition3) { return; }

  // lgica principal
}
```

### Tipos Explcitos (OBRIGATRIO)

```cpp
// ERRADO - tipos ambguos
int temperature;
unsigned count;

// CORRETO - tipos explcitos
int8_t temperature;
uint16_t count;
```

### MISRA-C:2012 Compliance

- Zero violations
- Usar `cppcheck --addon=misra.py` para validar
- Default case em todo switch
- No pointer arithmetic em ISRs

---

## Testando Contribuies

### Compilao

```bash
# Build para STM32F407
platformio run -e black_F407VE-EEPROM-SPI

# Verificar warnings (deve ser 0)
# Verificar Flash < 45%
# Verificar RAM < 20%
```

### Testes Unitrios

```bash
# Executar todos os testes
platformio test -e native

# Verificar: 313+ testes passando
```

### Checklist Pr-Commit

- [ ] Build SUCCESS
- [ ] Zero warnings
- [ ] Testes passando (313+)
- [ ] MISRA-C compliance (cppcheck)
- [ ] Complexidade < 10
- [ ] Guard clauses implementadas
- [ ] Tipos explcitos (uint8_t, etc.)
- [ ] Documentao atualizada

---

## Enviando Mudanas

### Formato de Commit (OBRIGATRIO)

```
tipo(escopo): descrio curta

Descrio detalhada do que mudou e por qu.
```

**Tipos vlidos:**
- `feat:` - Nova funcionalidade
- `fix:` - Correo de bug
- `refactor:` - Refatorao (sem mudana de comportamento)
- `docs:` - Documentao
- `test:` - Adio/modificao de testes
- `perf:` - Melhoria de performance

**Exemplo:**
```
refactor(corrections): extract helper for WUE calculation

Extract correctionWUE_calculate() helper function to reduce
cyclomatic complexity from 12 to 4. MISRA-C Rule 6-4-1 compliant.
```

### Pull Requests

1. Fork o repositrio
2. Crie branch: `git checkout -b feature/minha-feature`
3. Faa commits atmic (uma funcionalidade por commit)
4. Push: `git push origin feature/minha-feature`
5. Abra PR com descrio clara

**Template de PR:**
```markdown
## Resumo
Breve descrio da mudana.

## Motivao
Por que essa mudana  necessria?

## Testes
- [ ] Build SUCCESS
- [ ] Testes passando
- [ ] MISRA-C compliance

## Screenshots/Logs
(se aplicvel)
```

---

## Arquitetura do Projeto

### Padro Modular

Este projeto segue o padro **Interface + Registry + Coordinator**:

```
module/
 module_interface.h          // Contrato (struct com function pointers)
 module_registry.h/cpp       // Lookup O(1) via array const
 module_coordinator.h/cpp    // Orquestrao e dispatch
 submodules/                 // Implementaes
```

### Mdulos Existentes

| Mdulo | Diretrio | Status |
|--------|----------|--------|
| Decoders | `speeduino/decoders/` | 100% MISRA |
| Corrections | `speeduino/corrections/` | 100% MISRA |
| Sensors | `speeduino/sensors/` | 100% MISRA |
| Auxiliaries | `speeduino/auxiliaries/` | 100% MISRA |
| Schedulers | `speeduino/schedulers/` | 100% MISRA |

### Adicionando Novas Funcionalidades

1. Identifique o mdulo apropriado
2. Crie helper function se necessrio (para reduzir complexidade)
3. Siga o padro existente do mdulo
4. Adicione testes unitrios
5. Atualize documentao

---

## Convenes de Nomenclatura

### Variveis

```cpp
// Nomes descritivos
uint16_t engineRPM;           // NO: rpm
int8_t coolantTemperature;    // NO: clt
bool isEngineCranking;        // NO: cranking
```

### Funes

```cpp
// Helper functions: verbNoun
uint16_t calculateWarmupEnrichment(void);
bool checkRevLimit(void);
void applyFuelCorrection(void);

// Coordinator functions: prefixo module
void correctionsCoordinatorInitialize(void);
uint16_t decoderCoordinatorGetRPM(void);
```

### Constantes

```cpp
// UPPER_CASE com prefixo descritivo
static const uint16_t RPM_CRANKING_THRESHOLD = 400U;
static const int8_t CLT_COLD_ENGINE_C = 60;
#define INJ_CHANNEL_COUNT 8
```

---

## ISR Guidelines (CRTICO)

### Performance Requirements

- **Mximo:** 10s
- **Target:** < 5s
- **Proibido:** Alocao dinmica, strings, printf

### Boas Prticas

```cpp
// CORRETO - cached pointer, O(1) dispatch
static const DecoderInterface* activeDecoder;

void decoderISR(void) {
  activeDecoder->primaryISR();  // Fast: 1-2 cycles
}

// ERRADO - switch-case em ISR
void decoderISR(void) {
  switch (decoderType) {  // Slow: O(n) comparisons
    case TYPE_1: ...
  }
}
```

### Variveis Volteis

```cpp
// ISR shared variables MUST be volatile
volatile uint32_t toothCount;
volatile uint16_t revolutionTime;
```

---

## Licenciamento

Todas as contribuies so feitas sob **GNU GPLv3**, compatvel com Speeduino original.

Ao contribuir, voc concorda com o [Contributor License Agreement](https://github.com/noisymime/speeduino/wiki/Contributor-License-Agreement) do projeto Speeduino.

---

## Dvidas?

- Leia a documentao em `docs/`
- Junte-se ao Discord do Speeduino
- Abra uma issue no repositrio

---

**ltima Atualizao:** 2025-12-30
**Projeto:** SCG-ECU 2.0 - Speeduino Modularizado para STM32F407VGT6
