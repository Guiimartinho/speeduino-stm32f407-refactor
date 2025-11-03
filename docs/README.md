# DOCUMENTAÇÃO SCG-ECU 2.0
## STM32F407VGT6 8x8 - Speeduino Modularizado

**Versão Projeto:** 2.0
**Última Atualização:** 02/11/2025
**Status:** 100% COMPLETO (7/7 módulos principais)

---

## 📚 ESTRUTURA DA DOCUMENTAÇÃO

Documentação consolidada e simplificada. Apenas 11 arquivos essenciais (7 projeto + 4 aplicação VW Gol).

---

## 🎯 DOCUMENTOS PRINCIPAIS DO PROJETO (7 arquivos)

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

### 7. README.md (este arquivo)
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
├── PROJETO - DOCUMENTOS PRINCIPAIS (7)
│   ├── PROJETO_SCG_ECU_MASTER_REFERENCE.md    ← Documento mestre
│   ├── REQUISITOS_TECNICOS.md                 ← Padrões código
│   ├── IMPLEMENTACAO_MODULARIZACAO_STATUS.md  ← Status atual
│   ├── ARQUIVOS_PENDENTES_REFATORACAO.md      ← Roadmap futuro
│   ├── REVISAO_COMPLETA_MODULOS_1_6.md        ← Validação qualidade
│   ├── PHASE7_SCHEDULERS.md                   ← Exemplo módulo
│   └── README.md                              ← Este arquivo
│
└── VW GOL - APLICAÇÃO (4)
    ├── VW_GOL_INDEX.md                        ← Índice VW Gol
    ├── VW_GOL_AP18_COMPLETO.md               ← Doc completa
    ├── VW_GOL_COMPARATIVO_VERSOES.md         ← Comparativo
    └── VW_GOL_QUICK_REFERENCE.md             ← Cheat sheet
```

**Total:** 11 arquivos essenciais

---

## 📊 MÓDULOS COMPLETOS (7/7)

```
✅ Módulo 1: Board Configuration
✅ Módulo 2: Auxiliaries (10 subsistemas)
✅ Módulo 3: Decoders (32 decoders)
✅ Módulo 4: Corrections (25 funções)
✅ Módulo 5: Sensors (24 funções)
✅ Módulo 6: Table Access (tabelas 2D/3D)
✅ Módulo 7: Schedulers (fuel + ignition)

Status Build: ✅ SUCCESS
Flash Usage:  202KB / 524KB (38.6%)
RAM Usage:    21KB / 131KB (16.3%)
Warnings:     0
```

---

## 🔄 PRÓXIMAS FASES (Planejadas)

Ver: `ARQUIVOS_PENDENTES_REFATORACAO.md`

```
Fase 8:  Comunicação (comms_legacy, comms_CAN)
Fase 9:  Data Logging (SD_logger, logger)
Fase 10: Storage & Updates
Fase 11: Loop Principal & Proteções
Fase 12: Cálculos & Sensores
Fase 13: Utilitários & Cleanup
```

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

### 02/11/2025 - Consolidação Documentação
- Deletados 12 arquivos obsoletos
- Consolidados MISRA e WORKFLOWS em REQUISITOS
- Estrutura simplificada: 11 arquivos essenciais
- Atualizado este README

### 01/11/2025 - Aplicação VW Gol
- Adicionados 4 documentos VW Gol Quadrado AP 1.8
- Documentação completa aspirado + turbo
- Features opcionais (AC + Sequential)

### 29/10/2025 - Módulos 5-7 Completos
- Finalizados: Sensors, Table Access, Schedulers
- 7/7 módulos principais completos
- Build SUCCESS

---

**Versão:** 2.0
**Última Atualização:** 02/11/2025
**Mantido por:** Guiimartinho + Claude Code
