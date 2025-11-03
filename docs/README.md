# DOCUMENTAÇÃO SCG-ECU 2.0
## STM32F407VGT6 8x8 - Speeduino Modularizado

**Versão Projeto:** 2.0
**Última Atualização:** 02/11/2025
**Status:** ORGANIZADO (100%) mas NÃO REFATORADO (~35% compliance)

⚠️ **ATUALIZAÇÃO 02/11/2025:** Validação de código real revelou discrepância. Ver VALIDACAO_CODIGO_REAL.md

---

## 📚 ESTRUTURA DA DOCUMENTAÇÃO

Documentação consolidada e simplificada. Total de 13 arquivos essenciais (9 projeto + 4 aplicação VW Gol).

⚠️ **Novos documentos (02/11/2025):**
- VALIDACAO_CODIGO_REAL.md - Análise real do código vs documentação
- PLANO_ACAO_REFATORACAO_REAL.md - Roadmap de 30 semanas

---

## 🎯 DOCUMENTOS PRINCIPAIS DO PROJETO (9 arquivos)

### 1. PROJETO_SCG_ECU_MASTER_REFERENCE.md
**Descrição:** Documento mestre de referência do projeto completo
**Conteúdo:**
- Histórico do projeto
- Arquitetura geral
- Status de todos os módulos
- Próximos passos

**Quando ler:** Visão geral do projeto, antes de qualquer modificação

---

### 2. REQUISITOS_TECNICOS.md
**Descrição:** Padrões de código obrigatórios
**Conteúdo:**
- Guard clauses obrigatórias
- Complexidade ciclomática < 10
- Aninhamento ≤ 3 níveis
- ISR performance (< 10µs)
- MISRA C:2012 compliance

**Quando ler:** SEMPRE antes de escrever código

---

### 3. IMPLEMENTACAO_MODULARIZACAO_STATUS.md
**Descrição:** Status atual da implementação
**Conteúdo:**
- Progresso dos 7 módulos (100% completo)
- Métricas de build
- Flash/RAM usage
- Build time

**Quando ler:** Acompanhamento diário do progresso

---

### 4. ARQUIVOS_PENDENTES_REFATORACAO.md
**Descrição:** Roadmap de refatoração futura
**Conteúdo:**
- 37 arquivos auxiliares pendentes
- Priorização (Alta/Média/Baixa)
- Fases 8-13 planejadas
- Estimativas de tempo

**Quando ler:** Planejamento de próximas refatorações

---

### 5. REVISAO_COMPLETA_MODULOS_1_6.md
**Descrição:** Validação completa dos módulos 1-6
**Conteúdo:**
- Compliance check completo
- Métricas de qualidade
- Guard clauses verificadas
- ISR performance validada

**Quando ler:** Validação de qualidade, referência de padrões aplicados

---

### 6. PHASE7_SCHEDULERS.md
**Descrição:** Documentação detalhada do Módulo 7 (Schedulers)
**Conteúdo:**
- Implementação completa
- Padrões aplicados
- Métricas de performance
- Exemplo de módulo completo

**Quando ler:** Referência de como implementar módulos

---

### 7. VALIDACAO_CODIGO_REAL.md
**Descrição:** Validação do código real vs documentação (02/11/2025)
**Conteúdo:**
- Análise detalhada de código fonte
- Evidências de violações de padrões
- Comparação status documentado vs real
- Compliance real: ~35%

**Quando ler:** ANTES de qualquer nova refatoração - mostra a realidade do código

---

### 8. PLANO_ACAO_REFATORACAO_REAL.md
**Descrição:** Roadmap realista de refatoração (30 semanas)
**Conteúdo:**
- Fases detalhadas (A-D)
- Estratégia específica por módulo
- Templates de refatoração
- Ferramentas e scripts
- Métricas de sucesso

**Quando ler:** Planejamento de refatoração, alocação de recursos

---

### 9. README.md (este arquivo)
**Descrição:** Índice da documentação
**Conteúdo:** Guia de navegação dos documentos

---

## 🚗 DOCUMENTOS DA APLICAÇÃO VW GOL (4 arquivos)

### VW_GOL_INDEX.md
Índice completo da documentação VW Gol Quadrado AP 1.8

### VW_GOL_AP18_COMPLETO.md
Documentação técnica completa (144 KB, 5,380 linhas):
- Configurações aspirado e turbo (0.5/0.8/1.0 bar)
- Ar condicionado com controle ECU
- Câmbio sequencial + paddle shift
- Componentes críticos turbo
- BOM completo com 8 configurações

### VW_GOL_COMPARATIVO_VERSOES.md
Comparativo aspirado vs turbo + features opcionais

### VW_GOL_QUICK_REFERENCE.md
Guia de referência rápida (cheat sheet)

---

## 🚀 INÍCIO RÁPIDO

### Para Novos Desenvolvedores

```
1. Leia: PROJETO_SCG_ECU_MASTER_REFERENCE.md
   └─ Entenda o projeto completo

2. Leia: REQUISITOS_TECNICOS.md
   └─ Aprenda os padrões obrigatórios

3. Veja: IMPLEMENTACAO_MODULARIZACAO_STATUS.md
   └─ Conheça o status atual

4. Consulte: ARQUIVOS_PENDENTES_REFATORACAO.md
   └─ Veja o que falta fazer
```

### Para Implementar Novos Módulos

```
1. ✅ Consultar REQUISITOS_TECNICOS.md (padrões)
2. ✅ Ver PHASE7_SCHEDULERS.md (exemplo de módulo completo)
3. ✅ Seguir estrutura dos módulos existentes
4. ✅ Atualizar IMPLEMENTACAO_MODULARIZACAO_STATUS.md
5. ✅ Atualizar PROJETO_SCG_ECU_MASTER_REFERENCE.md
```

### Para Aplicação VW Gol

```
1. Começar por: VW_GOL_INDEX.md
2. Documentação completa: VW_GOL_AP18_COMPLETO.md
3. Decisão config: VW_GOL_COMPARATIVO_VERSOES.md
4. Referência rápida: VW_GOL_QUICK_REFERENCE.md
```

---

## 📂 ESTRUTURA DE ARQUIVOS

```
docs/
├── README.md                              ← VOCÊ ESTÁ AQUI
│
├── PROJETO - DOCUMENTOS PRINCIPAIS (9)
│   ├── PROJETO_SCG_ECU_MASTER_REFERENCE.md    ← Documento mestre
│   ├── REQUISITOS_TECNICOS.md                 ← Padrões código
│   ├── IMPLEMENTACAO_MODULARIZACAO_STATUS.md  ← Status atual (atualizado 02/11)
│   ├── ARQUIVOS_PENDENTES_REFATORACAO.md      ← Roadmap futuro
│   ├── REVISAO_COMPLETA_MODULOS_1_6.md        ← Validação qualidade
│   ├── PHASE7_SCHEDULERS.md                   ← Exemplo módulo
│   ├── VALIDACAO_CODIGO_REAL.md               ← ⚠️ NOVO: Análise real (02/11)
│   ├── PLANO_ACAO_REFATORACAO_REAL.md         ← ⚠️ NOVO: Roadmap 30 semanas
│   └── README.md                              ← Este arquivo
│
└── VW GOL - APLICAÇÃO (4)
    ├── VW_GOL_INDEX.md                        ← Índice VW Gol
    ├── VW_GOL_AP18_COMPLETO.md               ← Doc completa
    ├── VW_GOL_COMPARATIVO_VERSOES.md         ← Comparativo
    └── VW_GOL_QUICK_REFERENCE.md             ← Cheat sheet
```

**Total:** 13 arquivos essenciais (9 projeto + 4 VW Gol)

---

## 📊 STATUS REAL DOS MÓDULOS (Atualizado 02/11/2025)

```
ORGANIZAÇÃO: 7/7 ✅ (estrutura modular criada)
REFATORAÇÃO: 0/7 ❌ (código não migrado/refatorado)

⚠️ Módulo 1: Board Configuration    (~50% compliance - não verificado)
⚠️ Módulo 2: Auxiliaries            (~40% compliance - parcial)
❌ Módulo 3: Decoders               (10% - APENAS ESTRUTURA)
⚠️ Módulo 4: Corrections            (30% - wrappers + guard clauses parciais)
❌ Módulo 5: Sensors                (15% - APENAS ESTRUTURA)
⚠️ Módulo 6: Table Access           (~50% - não verificado)
⚠️ Módulo 7: Schedulers             (~40% - não verificado)

Compliance Média:                    ~35%
Código Migrado:                      Não (arquivos vazios/wrappers)
Funções < 50 linhas:                 20% (50+ violações)
Complexidade < 10:                   ~30% (30+ violações)
Guard clauses:                       30% aplicadas

Build Status:                        ✅ SUCCESS
Flash Usage:                         202KB / 524KB (38.6%)
RAM Usage:                           21KB / 131KB (16.3%)
Warnings:                            0

📄 Detalhes: VALIDACAO_CODIGO_REAL.md
📄 Roadmap:  PLANO_ACAO_REFATORACAO_REAL.md (30 semanas)
```

---

## 🔄 PRÓXIMAS FASES (Roadmap Real - 30 semanas)

Ver: `PLANO_ACAO_REFATORACAO_REAL.md` para detalhes completos

```
FASE A:  Análise Completa                (1 semana)   ← PRÓXIMO
FASE B:  Priorização                     (3 dias)
FASE C1: Decoders                        (6-8 semanas) - CRÍTICO (ISRs)
FASE C2: Corrections                     (4-6 semanas) - Migrar + refatorar
FASE C3: Communications                  (4 semanas)   - comms_legacy, CAN
FASE C4: Sensors                         (2-3 semanas)
FASE C5: Schedulers                      (2-3 semanas)
FASE C6: Outros Módulos                  (2-3 semanas)
FASE D:  Validação Final                 (2-3 semanas)

Total Estimado: 23-31 semanas (~6 meses)
```

**Arquivos Auxiliares Pendentes:** 37 arquivos (ver ARQUIVOS_PENDENTES_REFATORACAO.md)

---

## 📖 REFERÊNCIAS EXTERNAS

### Projeto Base
- Speeduino: https://speeduino.com
- Forum: https://speeduino.com/forum
- GitHub: https://github.com/noisymime/speeduino

### Plataforma
- STM32F407VGT6: https://www.st.com/en/microcontrollers-microprocessors/stm32f407vg.html
- PlatformIO: https://platformio.org

### Padrões
- MISRA C:2012: https://www.misra.org.uk
- Automotive Standards

---

## 📝 HISTÓRICO DE MUDANÇAS

### 02/11/2025 - VALIDAÇÃO CRÍTICA DO CÓDIGO REAL
⚠️ **DESCOBERTA IMPORTANTE:** Análise de código revelou discrepância entre documentação e realidade
- Criado: VALIDACAO_CODIGO_REAL.md (análise detalhada)
- Criado: PLANO_ACAO_REFATORACAO_REAL.md (roadmap 30 semanas)
- Atualizado: IMPLEMENTACAO_MODULARIZACAO_STATUS.md (status real ~35%)
- Status real: Organizado mas NÃO refatorado
- Código permanece em arquivos monolíticos
- 50+ violações de REQUISITOS_TECNICOS.md identificadas
- Roadmap realista: 6 meses para compliance real

### 02/11/2025 - Consolidação Documentação (manhã)
- Deletados 12 arquivos obsoletos
- Consolidados MISRA e WORKFLOWS em REQUISITOS
- Estrutura simplificada: agora 13 arquivos essenciais
- Atualizado este README

### 01/11/2025 - Aplicação VW Gol
- Adicionados 4 documentos VW Gol Quadrado AP 1.8
- Documentação completa aspirado + turbo
- Features opcionais (AC + Sequential)

### 29/10/2025 - Módulos "Completos" (revisão necessária)
- ⚠️ Status documentado: 7/7 módulos completos
- ⚠️ Status real (02/11): Apenas estrutura criada
- Build SUCCESS (código funcional mantido)

---

**Versão:** 2.0
**Última Atualização:** 02/11/2025
**Mantido por:** Guiimartinho + Claude Code
