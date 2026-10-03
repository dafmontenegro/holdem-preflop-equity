# Prompt para Claude Code — Proyecto "Probabilidades preflop de Texas Hold'em"

> Copia todo este archivo como primer mensaje de la sesión. La carpeta `poker-preflop-insumos/` (donde está este archivo) debe estar disponible en el entorno de trabajo; contiene datos, código y documentos de una fase de exploración previa.

---

## 0. Rol y reglas de trabajo

Eres un ingeniero de software con criterio matemático sólido en probabilidad, combinatoria y simulación, y con buen gusto en desarrollo web y visualización. Vas a convertir una investigación ya iniciada en (1) una investigación extendida y rigurosa, (2) documentación excelente y (3) un proyecto nuevo con herramienta interactiva en mi sitio web personal.

Reglas no negociables:

1. **No empieces de cero.** Ya existe una fase de exploración con resultados validados (secciones 3 a 6). Primero lee y reproduce; después extiende. Si encuentras un error en lo existente, documéntalo y corrígelo explícitamente: no lo ocultes.
2. **Ningún número "de memoria".** Toda cifra que aparezca en la documentación o en la herramienta debe salir de código que se ejecutó y que queda en el repositorio, reproducible con un comando.
3. **Separa siempre** lo exacto (enumeración), lo estimado (Monte Carlo, con su error) y los supuestos de modelado.
4. **Si algo de lo que pido está mal planteado matemáticamente, dímelo** en lugar de obedecer.
5. **Git: nunca hagas commit ni push.** Al final de cada fase entrégame el mensaje de commit propuesto y la lista de archivos tocados; yo hago los commits. Sin líneas de atribución ni de co-autoría en los mensajes.
6. **Pregunta antes de decisiones estructurales** del sitio (stack, rutas, dependencias nuevas, diseño general). Para decisiones menores, decide, sigue y documenta el supuesto.
7. Todo el contenido visible va en **español** (preguntaré si quiero versión en inglés).

---

## 1. Contexto

### 1.1 Sobre mí y el sitio

- Soy Daniel Felipe Montenegro Herrera, ingeniero de sistemas (Universidad Nacional de Colombia).
- Sitio personal: **montenegrodanielfelipe.com**, repositorio **dafmontenegro/dafmontenegro.github.io** (GitHub Pages, hosting estático: sin backend).
- El sitio tiene una **sección de proyectos**. Ya hay al menos otro proyecto interactivo planeado o publicado ahí (Super Pony Picker, una carrera de caballos 8-bit en p5.js). **Antes de proponer nada, inspecciona el repositorio**: estructura, framework o generador (si lo hay), cómo se registran los proyectos, estilos, convenciones de rutas, SEO y metadatos. El nuevo proyecto debe integrarse siguiendo esas mismas convenciones.

### 1.2 Origen de la investigación

Quería entender las probabilidades del póker (Texas Hold'em No Limit) con rigor y, en particular, el **preflop**: con dos cartas en la mano y *n* rivales, ¿vale la pena jugar? ¿Qué tan buena es mi mano? ¿Cómo cambia todo con 2 o con 9 jugadores? Y el caso de **ir all-in antes del flop**.

Durante la exploración aclaramos varias confusiones que la documentación final debe resolver bien, porque son las que tendría cualquier lector:

- Un all-in preflop **no** se decide "con las dos cartas": las cartas se voltean y **luego se reparte la mesa completa** (flop, turn, river). Gana la mejor mano de 5 entre 7 cartas. Solo hay "ganar sin tablero" si todos se retiran.
- Los rivales pueden retirarse ante un all-in: el shove gana por dos vías (fold de todos, o ganar el showdown).
- Stacks distintos generan **botes laterales**: cada jugador solo gana de cada rival hasta lo que él mismo puso. Ejemplo trabajado: A 20k, B 11k, C 20k all-in → bote principal 33k (A, B, C), lateral 18k (solo A y C).
- Las **cartas quemadas** no cambian ninguna probabilidad (son cartas desconocidas al azar); existen contra trampas y marcas.
- Las cartas **no son eventos independientes** (reparto sin reemplazo). Las manos de varios rivales tampoco lo son (comparten mazo).
- **Blockers / efecto de las cartas propias:** con A-A en la mano solo queda 1 combinación de A-A rival entre C(50,2) = 1.225.
- "Probabilidad de que un rival tenga algo mejor" preflop no está bien definida sin tablero. La definición rigurosa adoptada: un rival "me gana" si mi **equity contra esa mano concreta** es < 50 %.
- **Equity ≠ potencial.** Potencial = qué tan seguido llega una mano a cada categoría (escalera, color…). Equity = parte del bote que ganas en promedio. 32o arma más escaleras que AKo, pero pierde mucho más.
- **Empates:** solo hay empate si las 5 mejores cartas coinciden en valor (los kickers ya se resuelven antes). En un empate entre k jugadores te llevas 1/k del bote: no es una aproximación, es el pago real. Reportar siempre victoria, empate y derrota por separado.

### 1.3 Nivel del lector objetivo

El lector (yo, y cualquiera que llegue al sitio) sabe programar pero **no es experto en póker ni en combinatoria**. Pedí explicaciones "con el máximo detalle", ejemplos trabajados a mano y cero ambigüedad. Me molestan las respuestas vagas, los números sin método y los términos sin definir.

---

## 2. Insumos en `poker-preflop-insumos/`

| Ruta | Qué es |
|---|---|
| `data/potencial_169_conteos_exactos.csv` | Para cada una de las 169 manos: conteos **exactos** de la categoría final al river sobre los 2.118.760 tableros posibles (columnas: carta_alta … escalera_color), más `mejora_mesa` (tableros en que la categoría con 7 cartas supera la de la mesa sola). `combos` = 6/4/12. |
| `data/equity_169_vs_1a8_rivales_montecarlo.csv` | Equity (0–1) de las 169 manos contra 1..8 rivales con manos al azar. Monte Carlo, 100.000 repartos por celda, error estándar ≤ 0,16 pp. |
| `src/potencial_exacto.c` | Programa C que generó los conteos exactos (evaluador de **categoría** de 7 cartas con máscaras de bits; 5 bucles a<b<c<d<e). Salida sin cabecera: `./potencial_exacto > raw.csv`. Tarda ~17 s. |
| `src/equity_montecarlo.c` | Programa C de equity Monte Carlo (evaluador **completo** de 7 cartas con kickers; Fisher-Yates parcial; xorshift64; empates como 1/(k)). Uso: `./equity_montecarlo 100000 > eq8.csv`. ~70 s. |
| `scripts/build_xlsx_potencial.py` | Genera la hoja de cálculo del potencial a partir de `raw.csv` (formato de salida directo del C, sin cabecera). Incluye la validación exacta contra las 133.784.560 manos de 7 cartas. |
| `scripts/build_pdf_potencial.py`, `scripts/figs_heatmaps.py` | PDF de 18 páginas: método, algoritmo, cálculos a mano, validación y resultados del potencial. Esperan `raw.csv` en el directorio de trabajo. |
| `scripts/build_pdf_equity.py` | PDF de equity de 2 a 9 jugadores (espera `eq8.csv`). |
| `scripts/*_treys.py` | Exploraciones en Python con la librería `treys`: equity de ejemplos preflop/flop (Monte Carlo 40k), clasificación de 9♣8♣ contra las 1.225 manos rivales, efecto de cartas reveladas por enumeración exacta. Rutas y semillas fijas dentro de cada script. |
| `docs/probabilidades_preflop_holdem.pdf` | Documento explicativo del potencial (el más detallado). **Léelo completo**: define el tono y el nivel de explicación esperados. |
| `docs/equity_preflop_2_a_9_jugadores.pdf` | Tablas de equity por número de jugadores, guía de lectura y reglas. |
| `docs/potencial_preflop_169_manos.xlsx` | Hoja con conteos, probabilidades por fórmula, mapas y validación. |
| `prototipo/entrenador_preflop.html` | Prototipo autocontenido del entrenador interactivo (cuadrícula 13×13 coloreada, selector de 1–8 rivales, detalle por casilla, modo "te reparto dos cartas: ¿jugar o retirarse?"). Es una base funcional, no el diseño final. |

Requisitos para reproducir: `gcc`, Python 3 con `treys`, `openpyxl`, `reportlab`, `matplotlib`, `numpy`.

---

## 3. Resultados ya obtenidos (para verificar, no para copiar a ciegas)

### 3.1 Combinatoria base

- C(52,2) = 1.326 manos iniciales → **169 tipos** por simetría de palos: 13 parejas (6 combos), 78 suited (4 combos), 78 offsuit (12 combos). 78 + 312 + 936 = 1.326.
- Tableros tras ver tus 2 cartas: C(50,5) = **2.118.760**. Con orden serían 50·49·48·47·46 = 254.251.200 = 2.118.760 × 5!. Para la mano final al river el orden no importa; la probabilidad es idéntica con ambos conteos.
- Validación global del potencial: Σ sobre las 1.326 manos de los conteos por categoría = 21 × (conteo clásico de manos de 7 cartas), porque cada conjunto de 7 cartas se parte de C(7,2) = 21 formas en "2 privadas + 5 de mesa". **Coincide exacto** en las 9 categorías: carta alta 23.294.460; pareja 58.627.800; doble pareja 31.433.400; trío 6.461.620; escalera 6.180.020; color 4.047.644; full 3.473.184; póker 224.848; escalera de color 41.584 (total 133.784.560).

### 3.2 Cálculos a mano que coinciden con el programa

- **Color con 9♣8♣:** quedan 11 tréboles y 39 no-tréboles. ≥ 3 tréboles en la mesa: C(11,3)·C(39,2) + C(11,4)·39 + C(11,5) = 122.265 + 12.870 + 462 = 135.597. **Más** el caso olvidado con facilidad: mesa con 5 cartas de otro palo, 3·C(13,5) = 3.861. Total 139.458 → **6,58 %**. Programa: color 135.240 + escalera de color 4.218 = 139.458 ✓. (Con 7 cartas no puede haber color y full a la vez.)
- **Emparejar al menos una carta con 9-8 por el river:** 1 − C(44,5)/C(50,5) = 1 − 1.086.008/2.118.760 = **48,74 %**.
- **Una de tus dos cartas empareja en el flop:** 1 − C(44,3)/C(50,3) ≈ 32,4 %. **Pareja en mano hace trío en el flop:** 1 − C(48,3)/C(50,3) ≈ 11,8 %.
- **Outs:** proyecto de color en el flop (9 outs): 1 − (38/47)(37/46) = 34,97 %; escalera abierta (8 outs): 31,45 %. Regla del 4 y del 2.

### 3.3 Potencial (exacto, categoría final al river)

| Mano | Escalera o mejor | Color o mejor | Trío o mejor |
|---|---|---|---|
| T9s, 98s, JTs (suited conectadas medias) | 17,46 % | 8,93 % | 21,7 % |
| QJs | 15,6 % | 8,93 % | 19,9 % |
| AKs | 12,0 % | 8,93 % | 16,4 % |
| AA, KK, 22 | 12,58 % | 11,36 % | 24,35 % |
| TT, 55 | 13,71 % | 11,36 % | 25,41 % |
| 7-2 offsuit | 7,0 % | 4,3 % | 11,5 % |
| K-2 offsuit | 6,3 % (mínimo) | 4,3 % | — |

Hallazgos: el palo domina el color (todas las suited 8,93 %, todas las offsuit 4,32 %); las parejas tienen igual color o mejor (11,36 %, inflado por full house) pero **no** igual escalera: un 10 o un 5 está en 5 de las 10 escaleras posibles, un As, una K o un 2 en solo 2. "Ventanas" de escalera que contienen ambas cartas: conectadas 5–J → 4; un hueco → 3; AK → 1; 72 → 0.

**Errata ya corregida en la exploración:** se afirmó primero que todas las parejas tenían el mismo potencial; es falso (ver arriba). La documentación final debe dejarlo claro.

Limitación conocida: las categorías incluyen manos que vienen enteras en la mesa ("doble pareja o mejor" de 7-2 offsuit = 34 %). La columna `mejora_mesa` es una aproximación (no detecta mejoras dentro de la misma categoría, p. ej. un color más alto).

### 3.4 Equity contra rivales con manos al azar (Monte Carlo)

Ejemplos (de `data/equity_169_vs_1a8_rivales_montecarlo.csv`):

| Mano | 1 rival | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---|---|---|---|---|---|---|---|---|
| AA | 85,3 | 73,6 | 64,0 | 55,8 | 48,9 | 43,6 | 38,7 | 34,6 |
| 98s | 50,5 | 36,0 | 28,5 | 23,8 | 20,4 | 17,8 | 15,8 | 14,6 |
| 22 | 50,4 | 30,8 | 22,1 | 18,0 | 15,6 | 14,1 | 13,2 | 12,5 |
| 72o | 34,7 | 20,7 | 14,2 | 10,9 | 8,6 | 7,3 | 6,2 | 5,3 |

- **Validación:** el promedio ponderado por combos de las 169 manos da la parte justa 1/(n+1) en los 8 casos (diferencias ≤ 0,02 pp).
- Concuerda con valores publicados (AA ≈ 85,2 %, 72o ≈ 34,6 %), dentro del error.

**Regla de decisión usada ("parte justa"):** índice = equity / (100 / jugadores). Fuerte ≥ 1,4; jugable 1,1–1,4; marginal 0,95–1,1; retirarse < 0,95. En todas las mesas, ~30–32 % de las combinaciones son "fuerte o jugable"; lo que cambia es **cuáles**.

Reglas memorizables derivadas (validar con los datos y documentar dónde fallan):
1. Dos cartas de T a A → jugable con cualquier número de rivales (las 15 combinaciones, suited u offsuit).
2. Pareja 55+ → jugable siempre; 99+ fuerte siempre. 22–44: 44 jugable heads-up (22–33 marginales); malas con 3–5 rivales; jugables con 8 rivales.
3. Dos cartas ≤ 9, offsuit, sin pareja → retirarse en todas las mesas.
4. Pocos rivales favorece cartas altas (Axo, Kxo); muchos rivales favorece parejas y suited conectadas (98s jugable desde 3 rivales; 54s jugable con 8).

### 3.5 Otros resultados de la exploración

- **Equity en el flop** (treys, 40k): A♠K♠ en Q♠7♠2♦ → 72,6 % / 59,3 % / 44,7 % contra 1 / 2 / 5 rivales. A♥Q♦ en Q♠7♣2♥ → 87,4 % (1) y 68,4 % (3). K♠K♥ en A♣7♦2♥ → 79,6 % (1).
- **9♣8♣ contra las 1.225 manos rivales** (2.500 tableros por mano, umbral 47–53 %): rival favorito 516 combos (42,1 %), parejo 316 (25,8 %), 9♣8♣ favorito 393 (32,1 %). Con 2 rivales (pares de manos disjuntas, exacto sobre la clasificación): ninguno me gana 32,9 %, uno 49,9 %, ambos 17,2 %. Con 3/5/8 rivales: al menos uno me gana 81,7 % / 94,7 % / 99,3 % (la aproximación independiente 1−(1−p)ⁿ subestima ~1 pp). **Reemplazar por un cálculo exacto** (la clasificación tiene ruido cerca del umbral).
- **Cartas reveladas (enumeración exacta de C(48,5) = 1.712.304 tableros)**, 9♣8♣ contra:

| Rival muestra | Escalera o mejor | Color o mejor | Equity 9♣8♣ |
|---|---|---|---|
| (nada) | 17,46 % | 8,93 % | ≈ 50,7 % |
| A♥A♠ | 19,35 % | 9,90 % | 22,6 % |
| A♣K♣ | 16,35 % | 6,68 % | 34,8 % |
| 7♥7♦ | 16,07 % | 9,90 % | 49,1 % |
| 7♣6♣ | 14,22 % | 6,68 % | 66,5 % |

  Lección: cartas ajenas que no ves no cambian tus probabilidades; cartas reveladas sí (sacan cartas del mazo). Cartas inútiles para ti que salen del mazo **suben** tu potencial.

- **Orientación no verificada** (dada como regla práctica, NO calculada): "contra un all-in, pagar solo con JJ+ y AK". Verificarla o refutarla con el modelo de rangos de la fase 2.

---

## 4. Lo que quiero que hagas

### Fase 0 — Reconocimiento (sin escribir código del proyecto)

1. Lee todos los insumos (PDFs incluidos) y el repositorio del sitio.
2. Entrégame un **informe corto**: estructura del sitio, cómo se agregan proyectos, stack recomendado para la herramienta (justificado), riesgos, y una lista de preguntas abiertas. Espera mi respuesta antes de la fase 2 en adelante.

### Fase 1 — Reproducción y motor de cálculo

1. Crea un directorio de investigación (fuera de lo que se publica, o en la ruta que convenga según el repo) con un **motor reproducible**: evaluador de 7 cartas rápido (C, Rust o el lenguaje que justifiques; si compilas a WebAssembly para el navegador, mejor), tests unitarios del evaluador (escalera de color, rueda A-2-3-4-5, kickers en cada categoría, dos tríos → full, tres parejas → doble pareja con mejor kicker, empates por tablero).
2. Reproduce **exactamente** los conteos de `potencial_169_conteos_exactos.csv` y la validación de 133.784.560. Reproduce la equity Monte Carlo dentro del error.
3. Un único comando (`make`, script o similar) que regenere todos los datos.

### Fase 2 — Extender la investigación

En orden de prioridad:

1. **Matriz exacta heads-up 169 × 169**: equity, victoria, empate y derrota por enumeración completa de tableros, ponderando correctamente las combinaciones de palos entre las dos manos (usa isomorfismo de palos para no calcular combinaciones redundantes). Deriva de ahí la equity exacta contra mano aleatoria y compárala con el Monte Carlo.
2. **Equity contra 1–8 rivales** con precisión mayor (exacta donde sea factible; si no, Monte Carlo con intervalos de confianza y pruebas de convergencia contra los valores exactos).
3. **"¿Cuántos rivales tienen una mano que me gana?"** (definición de 1.2): distribución exacta de k = 0..n para cada mano, sin asumir independencia, comparada con la aproximación independiente. Reportar también la **distribución de equity** contra las 1.225 manos (bandas), no solo la clasificación binaria.
4. **Rangos**: ranking explícito de las 169 manos (equity vs aleatoria, documentado) y equity contra rivales que juegan "top X %" (X ∈ {5, 10, 15, 20, 30, 50, 100}).
5. **Decisión de all-in preflop**: EV del shove con bote muerto (ciegas), stack efectivo en big blinds, P(todos se retiran) como **parámetro** del usuario (no inventado), y equity multiway condicionada al rango de quien paga. Umbral de equity para pagar y P(fold) mínima que vuelve rentable el shove. Botes laterales en versión 2 (diseña para ello desde ya). Validar o refutar la regla "pagar con JJ+ y AK".
6. **Potencial refinado**: probabilidad de cada categoría **usando al menos una carta propia** (corrige la limitación de 3.3), y una métrica de "mejora real" que detecte mejoras dentro de la misma categoría.
7. (Opcional, si da tiempo) Potencial por calle (flop y turn): probabilidad de flopear pareja, proyectos de color, escaleras abiertas, etc.

Para cada resultado nuevo: invariantes (win+tie+lose = 1; simetría A vs B + B vs A = 1; promedio ponderado = 1/(n+1)), contraste con al menos una referencia externa (eval7, treys, OMPEval, PokerKit o una calculadora pública) y tiempo de cómputo.

### Fase 3 — Documentación

1. **Artículo principal** para el sitio, en español, nivel del lector de 1.3: del "qué es un flop" a la equity contra rangos. Debe incorporar y mejorar las explicaciones de `docs/probabilidades_preflop_holdem.pdf`: combinatoria con orden y sin orden, por qué 169, el algoritmo paso a paso (con pseudocódigo y el ejemplo de máscara de bits), el cálculo a mano del color, el complemento, la validación de 133.784.560, equity, parte justa, blockers, botes laterales, Monte Carlo vs exacto (con la fórmula del error √(p(1−p)/n)), y las reglas memorizables con su respaldo numérico.
2. **Sección de limitaciones honesta**: qué no modela (posición, ICM, rangos reales, juego postflop, stacks desiguales en v1).
3. **README técnico** del motor: cómo reproducir, estructura de datos, formato de los archivos, tests.
4. Datos descargables (CSV/JSON) con diccionario de columnas.

### Fase 4 — Herramienta interactiva en el sitio

Página de proyecto integrada con las convenciones del sitio. Debe funcionar perfecto en móvil (la mayoría de uso será en el teléfono), con tema claro/oscuro, accesible (teclado, contraste, etiquetas), rápida (datos precalculados y comprimidos; nada de recalcular 2 millones de tableros en el navegador salvo que uses WASM y se mantenga fluido).

Módulos (parte del prototipo `prototipo/entrenador_preflop.html` y mejóralo):

1. **Cuadrícula 13×13** coloreable por métrica seleccionable (equity vs n rivales, índice de parte justa, potencial por categoría, equity vs rango top X %), con leyenda clara y guía de lectura ("fila = carta alta, columna = carta baja, arriba suited, abajo offsuit, diagonal parejas").
2. **Selector de mano por cartas reales** (elige dos cartas con palo) → ficha de la mano: equity contra 1–8 rivales, índice de parte justa, potencial por categoría, manos que la dominan, regla memorizable que aplica.
3. **Entrenador**: te reparte dos cartas, decides jugar o retirarse (y, en modo avanzado, pagar un all-in o no contra un rango), feedback con números, racha y estadísticas. Sin almacenar datos personales; si guardas progreso, solo local.
4. **Calculadora cara a cara**: mano A vs mano B (o vs rango), con victoria, empate y derrota.
5. **Calculadora de all-in**: bote, ciegas, stack efectivo, número de rivales, rango de pago y P(fold) → EV y recomendación, mostrando la fórmula.
6. **Sección "Cómo se calculó"** enlazando al artículo y a los datos.

### Fase 5 — Publicación

SEO (título, descripción, Open Graph, datos estructurados si el sitio ya los usa), entrada en la lista de proyectos, enlaces cruzados con el artículo, prueba en móvil y escritorio, y revisión de rendimiento (Lighthouse o equivalente). Recuerda: no hagas commit ni push; entrégame mensaje y lista de archivos.

---

## 5. Criterios de aceptación

- [ ] Los conteos exactos del potencial se reproducen idénticos y pasan la validación de 133.784.560.
- [ ] La matriz 169×169 cumple simetría y su promedio da 50 % exacto.
- [ ] Toda cifra del artículo y de la herramienta sale de un archivo de datos generado por el motor.
- [ ] Cada tabla indica si es exacta o estimada, y con qué error.
- [ ] Las reglas memorizables están respaldadas por datos y sus excepciones documentadas.
- [ ] La regla "pagar all-in con JJ+ y AK" quedó verificada o refutada con números.
- [ ] La herramienta funciona en un teléfono de gama media sin bloquearse.
- [ ] El proyecto aparece en la sección de proyectos siguiendo las convenciones existentes del sitio.
- [ ] Recibí mensaje de commit y lista de archivos al final de cada fase.

## 6. Lo que NO quiero

- Explicaciones vagas, números sin método, o "aproximadamente" sin decir de dónde sale.
- Asumir independencia entre manos sin medir el error.
- Rehacer desde cero lo que ya está validado.
- Dependencias pesadas o servicios externos que no funcionen en GitHub Pages.
- Commits, pushes o líneas de co-autoría.

Empieza por la Fase 0.
