# GIT COMMIT RULES - MANDATORY

**LEIA ESTE DOCUMENTO ANTES DE QUALQUER COMMIT**

## REGRA ABSOLUTA DE COMMIT

### PROIBIDO - NEVER USE AUTHOR/CO-AUTHOR IN COMMIT MESSAGE

**NEVER INCLUDE:**
- Co-Authored-By: Claude <noreply@anthropic.com>
- Co-Authored-By: ANYTHING
- Author: ANYTHING
- Generated with Claude Code
- Generated with ANYTHING
- Any reference to AI/Claude/Assistant

### CORRECT COMMIT FORMAT

```bash
git commit -m "$(cat <<'EOF'
refactor: Brief description (FASE X)

Detailed description of what changed.

## Changes

### Section 1
- Point 1
- Point 2

### Section 2
- Point 3
- Point 4

## Impact

**Build Results:**
- RAM: X bytes
- Flash: X bytes
- Warnings: 0

**Code Metrics:**
- Lines: before -> after
- Complexity: before -> after

## Technical Details

Explanation of the implementation.

Follows project coding standards.
EOF
)"
```

### INCORRECT COMMIT FORMAT (DO NOT USE)

```bash
git commit -m "$(cat <<'EOF'
refactor: Description

... content ...

Generated with Claude Code  <--- FORBIDDEN

Co-Authored-By: Claude <noreply@anthropic.com>  <--- FORBIDDEN
EOF
)"
```

## COMMIT CHECKLIST

Before running git commit, verify:

- [ ] NO "Co-Authored-By" line
- [ ] NO "Generated with" line
- [ ] NO "Author:" line
- [ ] NO reference to AI/Claude/Assistant
- [ ] Commit message ends with technical details
- [ ] Last line is "Follows project coding standards." (period at end)

## PUNISHMENT FOR BREAKING THIS RULE

If you include author/co-author/generated-with:
- Commit will be rejected
- You must read this document again
- You must apologize
- You must never make this mistake again

## SUMMARY

**FORBIDDEN WORDS IN COMMIT MESSAGE:**
- Co-Authored-By
- Author:
- Generated with
- Claude
- AI
- Assistant

**ALWAYS END COMMIT WITH:**
```
Follows project coding standards.
```

**NOTHING AFTER THIS LINE**

---

**READ THIS DOCUMENT EVERY TIME BEFORE COMMIT**
