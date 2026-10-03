# Insumos — Probabilidades preflop de Texas Hold'em

Paquete de la fase de exploración. Empieza por `PROMPT_CLAUDE_CODE.md`: es el prompt para la sesión de Claude Code y explica cada archivo.

- `PROMPT_CLAUDE_CODE.md` — prompt completo (contexto, resultados, fases, criterios de aceptación).
- `data/` — resultados: potencial exacto (169 manos × 9 categorías) y equity Monte Carlo (169 manos × 1–8 rivales).
- `src/` — programas en C que generaron los datos.
- `scripts/` — scripts de Python (hoja de cálculo, PDFs, mapas de calor, exploraciones con treys).
- `docs/` — PDFs y hoja de cálculo generados.
- `prototipo/` — entrenador interactivo autocontenido (abrir en el navegador).

Reproducir los datos base:

    gcc -O2 -o potencial_exacto src/potencial_exacto.c && ./potencial_exacto > raw.csv      # ~17 s
    gcc -O2 -o equity_montecarlo src/equity_montecarlo.c && ./equity_montecarlo 100000 > eq8.csv   # ~70 s

`raw.csv` y `eq8.csv` salen sin cabecera; los scripts de `scripts/` esperan ese formato en el directorio de trabajo. Las versiones con cabecera están en `data/`.
