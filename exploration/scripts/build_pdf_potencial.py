import csv
from reportlab.lib.pagesizes import A4
from reportlab.platypus import (SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle,
    PageBreak, Image, Preformatted, KeepTogether)
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib import colors
from reportlab.lib.units import cm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
F="/usr/share/fonts/truetype/dejavu/"
pdfmetrics.registerFont(TTFont("DV",F+"DejaVuSans.ttf"))
pdfmetrics.registerFont(TTFont("DVB",F+"DejaVuSans-Bold.ttf"))
pdfmetrics.registerFont(TTFont("DVI",F+"DejaVuSans-Oblique.ttf"))
pdfmetrics.registerFont(TTFont("DVM",F+"DejaVuSansMono.ttf"))
from reportlab.pdfbase.pdfmetrics import registerFontFamily
registerFontFamily("DV",normal="DV",bold="DVB",italic="DVI",boldItalic="DVB")
NAVY=colors.HexColor("#1F3864")
body=ParagraphStyle("b",fontName="DV",fontSize=9.6,leading=14,spaceAfter=6,alignment=4)
h1=ParagraphStyle("h1",fontName="DVB",fontSize=15,leading=19,textColor=NAVY,spaceBefore=10,spaceAfter=8)
h2=ParagraphStyle("h2",fontName="DVB",fontSize=11.5,leading=15,textColor=NAVY,spaceBefore=8,spaceAfter=5)
title=ParagraphStyle("t",fontName="DVB",fontSize=22,leading=27,textColor=NAVY,spaceAfter=10)
sub=ParagraphStyle("s",fontName="DV",fontSize=11,leading=15,textColor=colors.HexColor("#444444"),spaceAfter=4)
box=ParagraphStyle("box",parent=body,backColor=colors.HexColor("#EEF3FA"),borderColor=colors.HexColor("#9DB5D8"),
    borderWidth=0.6,borderPadding=7,spaceBefore=6,spaceAfter=12)
eq=ParagraphStyle("eq",fontName="DV",fontSize=10.5,leading=16,alignment=1,spaceBefore=4,spaceAfter=8)
cell=ParagraphStyle("c",fontName="DV",fontSize=8.4,leading=10.5)
cellb=ParagraphStyle("cb",fontName="DVB",fontSize=8.4,leading=10.5,textColor=colors.white)
mono=ParagraphStyle("m",fontName="DVM",fontSize=7.4,leading=9.4,backColor=colors.HexColor("#F5F5F5"),
    borderPadding=5,spaceBefore=4,spaceAfter=10)
S=[]
P=lambda t,st=body: S.append(Paragraph(t,st))
def T(rows,widths,head=True,small=False):
    st=cell
    data=[[Paragraph(str(x),cellb if (head and i==0) else st) for x in r] for i,r in enumerate(rows)]
    t=Table(data,colWidths=widths,repeatRows=1 if head else 0)
    sty=[("GRID",(0,0),(-1,-1),0.4,colors.HexColor("#B0B7C3")),("VALIGN",(0,0),(-1,-1),"MIDDLE"),
         ("TOPPADDING",(0,0),(-1,-1),2.5),("BOTTOMPADDING",(0,0),(-1,-1),2.5)]
    if head: sty.append(("BACKGROUND",(0,0),(-1,0),NAVY))
    for i in range(1,len(rows)):
        if i%2==0: sty.append(("BACKGROUND",(0,i),(-1,i),colors.HexColor("#F3F6FA")))
    t.setStyle(TableStyle(sty)); S.append(t); S.append(Spacer(1,8))
def code(txt): S.append(Preformatted(txt,mono))

# datos
d={}; order=[]
for r in csv.reader(open('raw.csv')):
    key=r[0] if r[1]=="par" else r[0]+("s" if r[1]=="suited" else "o")
    v=[int(x) for x in r[2:]]; d[key]=v; order.append((key,r[1]))
def pc(x,n=2118760): return f"{x/n*100:.2f} %"
def fmt(x): return f"{x:,}".replace(",",".")

# ===== PORTADA
S.append(Spacer(1,3*cm))
P("Probabilidades preflop en Texas Hold'em",title)
P("Potencial de las 169 manos iniciales: método, algoritmo, cálculos a mano, resultados y validación",sub)
S.append(Spacer(1,0.6*cm))
P("Este documento explica desde cero cómo se calcularon las probabilidades de la hoja de cálculo <i>potencial_preflop_169_manos.xlsx</i>. "
  "Cada número se obtuvo por <b>enumeración exacta</b> (contando todos los casos posibles, sin simulación ni azar) y el resultado global se "
  "comprobó contra el conteo clásico de las 133.784.560 manos de 7 cartas: la coincidencia es exacta, carta por carta.",box)
P("<b>Contenido</b>",h2)
for t in ["1. Conceptos básicos del juego","2. Combinatoria desde cero: ¿por qué C(50,5) = 2.118.760 y no 254.251.200?",
          "3. ¿De dónde salen las 169 manos?","4. El algoritmo paso a paso (cómo se clasifica cada tablero)",
          "5. De conteos a probabilidades (el paso de dividir)","6. Cálculos a mano: color, pareja y escaleras",
          "7. Validación: cómo sabemos que no hay errores","8. Resultados de las 169 manos",
          "9. Potencial frente a equity, y el efecto de las cartas que ves","10. Qué NO mide este análisis y siguientes pasos",
          "Anexo A. Tabla completa de las 169 manos","Anexo B. Código fuente del cálculo"]:
    P(t,ParagraphStyle("toc",parent=body,spaceAfter=2,alignment=0))
S.append(PageBreak())

# ===== 1
P("1. Conceptos básicos del juego",h1)
P("En Texas Hold'em cada jugador recibe <b>2 cartas privadas</b> y en la mesa se revelan <b>5 cartas comunitarias</b> en tres etapas: el "
  "<b>flop</b> (3 cartas), el <b>turn</b> (1 carta) y el <b>river</b> (1 carta). Antes del flop se llama <b>preflop</b>. Al final, cada jugador "
  "arma su <b>mejor mano de 5 cartas</b> usando cualquier combinación de sus 7 cartas disponibles (sus 2 más las 5 de la mesa): puede usar "
  "sus dos cartas, una o ninguna.")
P("Las manos se ordenan en <b>categorías</b>, de la más débil a la más fuerte:")
T([["#","Categoría","Qué es","Ejemplo"],
   ["0","Carta alta","Nada combinado","A♠ J♦ 8♣ 5♥ 3♠"],
   ["1","Pareja","Dos cartas del mismo valor","9♠ 9♥ K♦ 7♣ 2♠"],
   ["2","Doble pareja","Dos parejas distintas","9♠ 9♥ 5♦ 5♣ K♠"],
   ["3","Trío","Tres del mismo valor","8♠ 8♥ 8♦ A♣ 4♠"],
   ["4","Escalera","Cinco valores consecutivos, palos cualquiera","5♠ 6♥ 7♦ 8♣ 9♠"],
   ["5","Color","Cinco cartas del mismo palo","K♣ 10♣ 7♣ 4♣ 2♣"],
   ["6","Full house","Trío + pareja","7♠ 7♥ 7♦ Q♣ Q♠"],
   ["7","Póker","Cuatro del mismo valor","J♠ J♥ J♦ J♣ 3♠"],
   ["8","Escalera de color","Escalera con todas del mismo palo","5♥ 6♥ 7♥ 8♥ 9♥"]],
  [0.8*cm,3.2*cm,6.8*cm,4.6*cm])
P("<b>Notación.</b> Los valores son 2, 3, 4, 5, 6, 7, 8, 9, T (diez), J, Q, K, A. Los palos son ♣ tréboles, ♦ diamantes, ♥ corazones y ♠ picas. "
  "Una mano se escribe con sus dos valores y una letra: <b>s</b> (<i>suited</i>, mismo palo, como 9♣8♣ = 98s), <b>o</b> (<i>offsuit</i>, palos "
  "distintos, como 9♣8♥ = 98o) o sin letra si es pareja (como AA). El As cuenta como el valor más alto y también como el más bajo en la "
  "escalera A-2-3-4-5.")
P("<b>Qué calculamos aquí.</b> Para cada mano inicial, la probabilidad de que, cuando salgan las 5 cartas de la mesa, tu mejor mano de 5 cartas "
  "caiga en cada categoría. Esto es el <b>potencial</b> o <b>versatilidad</b> de la mano. No es la probabilidad de ganar (eso es la "
  "<i>equity</i>, sección 9).")

# ===== 2
P("2. Combinatoria desde cero",h1)
P("2.1 Contar con orden y sin orden",h2)
P("Supón que tienes 3 cartas, A, B y C, y quieres sacar 2. Hay dos maneras de contar:")
P("• <b>Importando el orden</b> (permutaciones): AB, BA, AC, CA, BC, CB = 6 resultados. La primera carta tiene 3 opciones y la segunda 2, así "
  "que 3 × 2 = 6.<br/>• <b>Sin importar el orden</b> (combinaciones): {A,B}, {A,C}, {B,C} = 3 resultados. AB y BA son el mismo grupo de cartas.")
P("Cada grupo sin orden aparece varias veces en la lista con orden: tantas como formas de ordenar sus elementos. Dos cartas se ordenan de 2 × 1 = 2 "
  "formas, por eso 6 / 2 = 3. En general, <i>k</i> elementos se ordenan de <i>k!</i> formas (<i>k</i> factorial: k × (k−1) × … × 1). De ahí sale "
  "la fórmula del <b>número combinatorio</b>:")
P("C(n, k) = [n × (n−1) × … × (n−k+1)] / k!  =  n! / (k! · (n−k)!)",eq)
P("Se lee «n en k»: cuántos grupos distintos de <i>k</i> elementos puedes formar con <i>n</i>, sin importar el orden.")
P("2.2 Tu pregunta: ¿254.251.200 o 2.118.760?",h2)
P("Tienes tus 2 cartas, quedan 50 en el mazo y salen 5 a la mesa.")
P("• Con orden: 50 × 49 × 48 × 47 × 46 = <b>254.251.200</b> secuencias. Es correcto como número de <i>formas de repartir las 5 cartas en fila</i>.<br/>"
  "• Sin orden: esas 5 cartas se pueden ordenar de 5! = 5 × 4 × 3 × 2 × 1 = 120 formas, y todas dan exactamente la misma mesa final. "
  "Entonces 254.251.200 / 120 = <b>2.118.760</b> = C(50, 5).")
P("¿Por qué usamos la versión sin orden? Porque al llegar al river tu mano final depende <b>solo de cuáles</b> cartas hay en la mesa, no de en qué "
  "orden salieron. Si el flop es 7♦ K♠ 2♣ y luego salen 5♥ y J♣, la mano final es idéntica a la de un flop 2♣ J♣ 5♥ con turn K♠ y river 7♦.",box)
P("Las dos formas de contar dan <b>la misma probabilidad</b>, porque cada mesa sin orden aparece exactamente 120 veces en la lista con orden; al dividir "
  "casos favorables entre casos totales, el 120 se cancela arriba y abajo. Contar sin orden simplemente hace 120 veces menos trabajo.")
P("<b>Cuándo sí importaría el orden:</b> si quieres analizar decisiones por calle (qué pasa en el flop, qué pasa en el turn), porque ahí importa "
  "qué cartas se conocen en cada momento. Para la mano final al river, no.")
P("2.3 Números que usaremos",h2)
T([["Expresión","Valor","Qué cuenta"],
   ["C(52, 2)","1.326","Manos iniciales posibles (2 cartas de 52)"],
   ["C(50, 5)","2.118.760","Mesas posibles tras quitar tus 2 cartas"],
   ["C(52, 7)","133.784.560","Conjuntos de 7 cartas (validación)"],
   ["C(13, 2)","78","Pares de valores distintos (p. ej. {9, 8})"],
   ["C(4, 2)","6","Formas de elegir 2 palos de 4"],
   ["C(11, 3)","165","Grupos de 3 tréboles entre 11"],
   ["C(39, 2)","741","Grupos de 2 cartas entre 39"]],
  [3.2*cm,3*cm,9.2*cm])

# ===== 3
P("3. ¿De dónde salen las 169 manos?",h1)
P("Hay C(52, 2) = <b>1.326</b> manos iniciales concretas. Pero muchas son estratégicamente idénticas: 9♣8♣ y 9♥8♥ se comportan igual, porque los "
  "palos no tienen jerarquía. Solo importa si las dos cartas son del mismo palo o no. Agrupando por esa simetría quedan tres familias:")
T([["Familia","Cuántos tipos","Por qué","Combos por tipo","Combos totales"],
   ["Parejas (AA … 22)","13","Uno por cada valor","C(4,2) = 6","13 × 6 = 78"],
   ["Suited (AKs … 32s)","78","C(13,2) pares de valores distintos","4 (un palo de 4)","78 × 4 = 312"],
   ["Offsuit (AKo … 32o)","78","Los mismos 78 pares de valores","4 × 3 = 12","78 × 12 = 936"],
   ["Total","169","13 + 78 + 78","","1.326 ✓"]],
  [3.6*cm,2.2*cm,4.6*cm,2.8*cm,2.8*cm])
P("Los 12 combos offsuit: la carta alta puede ser de 4 palos y la baja de cualquiera de los otros 3, así que 4 × 3 = 12. Los 6 combos de pareja: "
  "de los 4 palos eliges 2 sin orden, C(4,2) = 6 (por ejemplo ♣♦, ♣♥, ♣♠, ♦♥, ♦♠, ♥♠).")
P("<b>Por qué basta calcular una mano representativa por tipo.</b> Si renombras los palos (por ejemplo, cambias todos los ♣ por ♥ y viceversa), cada "
  "mesa se convierte en otra mesa igual de probable y las categorías no cambian. Así que 9♣8♣ tiene exactamente las mismas probabilidades que 9♥8♥. "
  "El programa calcula 169 manos, no 1.326.")
S.append(Image("grid169.png",width=12*cm,height=11.2*cm))
P("<b>Cómo leer la cuadrícula 13 × 13:</b> las filas y columnas son los valores de A a 2. La diagonal son las parejas; arriba de la diagonal, las "
  "manos suited; abajo, las offsuit. Es la forma estándar de presentar las 169 manos, y la usan los mapas de calor de la sección 8.")

# ===== 4
S.append(PageBreak())
P("4. El algoritmo paso a paso",h1)
P("Sí, hay un algoritmo concreto. Es un programa en C (anexo B) que, para cada una de las 169 manos, hace esto:")
code("""para cada mano inicial (169):
    mazo  <- las 52 cartas menos tus 2               # quedan 50
    contador[0..8] <- 0                               # uno por categoría
    para cada mesa de 5 cartas del mazo (2.118.760):  # paso 3
        siete <- tus 2 cartas + las 5 de la mesa
        cat   <- categoria(siete)                     # 0 = carta alta ... 8 = escalera de color
        contador[cat] <- contador[cat] + 1
    probabilidad[cat] <- contador[cat] / 2.118.760    # paso 4""")
P("4.1 Representar las cartas como números",h2)
P("Cada carta es un número de 0 a 51: <b>carta = valor × 4 + palo</b>, con valor de 0 (el 2) a 12 (el As) y palo de 0 a 3. Por ejemplo, el 9 es el "
  "valor 7 y ♣ es el palo 0, así que 9♣ = 7 × 4 + 0 = 28. Para recuperar datos: valor = carta ÷ 4 (división entera) y palo = resto de carta ÷ 4.")
P("4.2 Recorrer cada mesa una sola vez",h2)
P("Las 50 cartas restantes se ponen en una lista con posiciones 0 a 49. Se usan cinco bucles anidados con posiciones <b>a &lt; b &lt; c &lt; d &lt; e</b>. "
  "Exigir que las posiciones vayan en orden creciente hace que cada grupo de 5 cartas aparezca <b>exactamente una vez</b>: el grupo {3, 10, 17, 22, 40} "
  "solo se genera en ese orden, nunca como {40, 3, …}. Es la división entre 120 de la sección 2, hecha directamente al recorrer.")
code("""for a in 0..49:
  for b in a+1..49:
    for c in b+1..49:
      for d in c+1..49:
        for e in d+1..49:
          mesa = (mazo[a], mazo[b], mazo[c], mazo[d], mazo[e])   # 2.118.760 veces en total""")
P("4.3 El paso 3: clasificar 7 cartas en una categoría",h2)
P("Con las 7 cartas, el programa arma tres resúmenes:")
P("• <b>cuenta[valor]</b>: cuántas cartas hay de cada valor (13 casillas).<br/>"
  "• <b>cuenta_palo[palo]</b>: cuántas cartas hay de cada palo (4 casillas).<br/>"
  "• <b>máscara de valores</b>: un número de 13 bits donde el bit <i>v</i> vale 1 si hay al menos una carta de valor <i>v</i>. Hay otra máscara igual "
  "por cada palo.")
P("Luego pregunta de <b>la categoría más fuerte a la más débil</b> y se queda con la primera que cumple. Revisar en ese orden es clave: si tienes trío y "
  "también color, lo que cuenta es el color.")
T([["Orden","Pregunta","Cómo se comprueba"],
   ["1","¿Escalera de color?","Algún palo con 5 o más cartas y, dentro de ese palo, 5 valores seguidos"],
   ["2","¿Póker?","Algún valor con cuenta 4"],
   ["3","¿Full house?","Un valor con cuenta 3 y otro con cuenta ≥ 2 (o dos tríos)"],
   ["4","¿Color?","Algún palo con cuenta_palo ≥ 5"],
   ["5","¿Escalera?","5 valores seguidos en la máscara general"],
   ["6","¿Trío?","Algún valor con cuenta 3"],
   ["7","¿Doble pareja?","Dos o más valores con cuenta 2"],
   ["8","¿Pareja?","Un valor con cuenta 2"],
   ["9","Si nada","Carta alta"]],
  [1.7*cm,3.4*cm,10.3*cm])
P("<b>Detectar una escalera con la máscara.</b> Ejemplo: tienes 9♣8♣ y la mesa es 7♦ 6♠ 5♥ K♣ 2♦. Los valores presentes son 2, 5, 6, 7, 8, 9, K. En la "
  "máscara (bit 0 = el 2, bit 12 = el As) queda así:")
code("""valor:   A K Q J T 9 8 7 6 5 4 3 2
bit:     0 1 0 0 0 1 1 1 1 1 0 0 1
                   ^^^^^^^^^  cinco unos seguidos: 5-6-7-8-9 -> ESCALERA""")
P("El programa detecta «cinco unos seguidos» con una operación de bits: toma la máscara <i>m</i> y calcula <i>m</i> AND (<i>m</i> desplazada 1) AND "
  "(desplazada 2) AND (desplazada 3) AND (desplazada 4). El resultado es distinto de cero solo si hay cinco bits consecutivos encendidos. Para la "
  "escalera baja A-2-3-4-5, antes se copia el bit del As a una posición por debajo del 2.")
P("<b>Verificación de la clasificación completa del ejemplo:</b> no hay palo con 5 cartas (los ♣ son 9, 8 y K: solo 3), no hay valor repetido, así que no "
  "hay póker, full, trío ni parejas; sí hay escalera. Categoría = 4 (escalera). El contador de escalera sube en 1.")
P("4.4 Dos recuentos por mesa",h2)
P("En cada mesa el programa clasifica también <b>la mesa sola</b> (5 cartas, sin las tuyas). Si tu categoría con 7 cartas es mayor que la de la mesa, "
  "cuenta como «tus cartas mejoran la mesa». Es la columna «Mejora al tablero» de la hoja.")
P("<b>Tiempo de cálculo:</b> 169 manos × 2.118.760 mesas ≈ 358 millones de clasificaciones. En C tarda unos 17 segundos.")

# ===== 5
P("5. De conteos a probabilidades (el paso 4)",h1)
P("Al terminar los bucles, cada contador dice en cuántas de las 2.118.760 mesas terminaste en esa categoría. Como todas las mesas son igual de probables "
  "(el mazo está bien barajado), la probabilidad es simplemente:")
P("P(categoría) = mesas donde terminas en esa categoría / 2.118.760",eq)
P("Ejemplo real con 98s (9♣8♣), valores exactos del programa:")
v=d["98s"]; cats=["Carta alta","Pareja","Doble pareja","Trío","Escalera","Color","Full house","Póker","Escalera de color"]
rows=[["Categoría","Mesas (conteo)","Probabilidad"]]
for i,c in enumerate(cats): rows.append([c,fmt(v[1+i]),pc(v[1+i])])
rows.append(["<b>Total</b>","<b>"+fmt(sum(v[1:10]))+"</b>","<b>100,00 %</b>"])
T(rows,[5*cm,4*cm,4*cm])
P("Las categorías son <b>excluyentes</b>: cada mesa se cuenta en una sola (la más alta que logres), por eso la suma da exactamente 2.118.760 y las "
  "probabilidades suman 100 %. Las columnas «o mejor» de la hoja se obtienen sumando: «escalera o mejor» = escalera + color + full + póker + escalera de "
  "color = 17,46 % en este caso.")

# ===== 6
S.append(PageBreak())
P("6. Cálculos a mano",h1)
P("La enumeración es la forma más segura, pero varios resultados se pueden deducir con fórmulas. Esto sirve para entender y para comprobar que el programa "
  "no se equivoca.")
P("6.1 El color con 9♣8♣, paso a paso",h2)
P("<b>Situación.</b> Tienes dos tréboles. En el mazo quedan 50 cartas: <b>11 tréboles</b> (13 menos tus 2) y <b>39 que no son tréboles</b>. Para tener color de "
  "tréboles necesitas 5 tréboles entre tus 7 cartas: como ya tienes 2, en la mesa deben salir <b>al menos 3 tréboles</b>.")
P("Separamos en casos según cuántos tréboles trae la mesa. Los casos no se pisan (una mesa tiene exactamente 3, o exactamente 4, o exactamente 5 tréboles), "
  "así que se pueden sumar sin contar nada dos veces.")
T([["Caso","Qué eliges","Cuenta","Resultado"],
   ["Exactamente 3 tréboles","3 tréboles de 11, y 2 cartas de las 39 no-tréboles","C(11,3) × C(39,2) = 165 × 741","122.265"],
   ["Exactamente 4 tréboles","4 tréboles de 11, y 1 carta de las 39","C(11,4) × C(39,1) = 330 × 39","12.870"],
   ["5 tréboles","los 5 de entre los 11","C(11,5)","462"],
   ["Subtotal (tu color de ♣)","","","135.597"]],
  [3.4*cm,5*cm,4.4*cm,2.6*cm])
P("<b>Por qué se multiplica.</b> En el primer caso, cada grupo de 3 tréboles se puede combinar con cualquier grupo de 2 no-tréboles. Hay 165 formas de "
  "lo primero y 741 de lo segundo, y cada pareja de elecciones da una mesa distinta: 165 × 741 = 122.265 mesas.")
P("<b>Detalle de C(11,3):</b> (11 × 10 × 9) / (3 × 2 × 1) = 990 / 6 = 165. <b>C(39,2):</b> (39 × 38) / 2 = 741. <b>C(11,4):</b> (11 × 10 × 9 × 8) / 24 = 330. "
  "<b>C(11,5):</b> (11 × 10 × 9 × 8 × 7) / 120 = 462.")
P("<b>Un caso que suele olvidarse.</b> También tienes color si la mesa trae 5 cartas de <i>otro</i> palo (5 diamantes, por ejemplo): el color está en la mesa "
  "y tú lo usas. Para cada uno de los otros 3 palos quedan 13 cartas, y eliges 5: 3 × C(13,5) = 3 × 1.287 = <b>3.861</b> mesas. No se cruza con los casos "
  "anteriores, porque si hay 5 diamantes no caben 3 tréboles en 5 cartas.")
P("Total de mesas con color: 135.597 + 3.861 = 139.458.   P(color) = 139.458 / 2.118.760 = 6,58 %",box)
P("<b>Comprobación con el programa:</b> contó 135.240 mesas en la categoría «color» y 4.218 en «escalera de color» (que también son colores). "
  "135.240 + 4.218 = 139.458. Coincide exactamente. Además, con 7 cartas es imposible tener color y full a la vez, así que no hay colores escondidos "
  "en la categoría full.")
P("<b>¿Y el 8,9 % de «color o mejor» de la hoja?</b> Es otra pregunta: incluye también full house y póker, que se logran sin color (con parejas y tríos). "
  "6,58 % es «tener color»; 8,9 % es «terminar en color o en algo aún más fuerte».")
P("6.2 Emparejar al menos una de tus cartas",h2)
P("Con 9-8, ¿cuál es la probabilidad de que en la mesa salga al menos un 9 o un 8? Quedan 3 nueves y 3 ochos, es decir, 6 cartas «buenas» y 44 «otras». "
  "Es más fácil contar lo contrario: que <b>no</b> salga ninguna. Eso es elegir las 5 cartas solo entre las 44 otras:")
P("P(ninguna) = C(44,5) / C(50,5) = 1.086.008 / 2.118.760 = 51,26 %<br/>P(al menos una) = 1 − 51,26 % = <b>48,74 %</b>",eq)
P("Esta es la <b>regla del complemento</b>: «al menos uno» = 1 − «ninguno». Se usa todo el tiempo porque «ninguno» es un solo caso fácil de contar, mientras "
  "que «al menos uno» tiene muchos casos (1, 2, 3… cartas buenas).")
P("6.3 Por qué las conectadas tienen más escaleras",h2)
P("Hay 10 escaleras posibles: A-2-3-4-5, 2-3-4-5-6, …, T-J-Q-K-A. Una mano aporta sus dos cartas a una escalera solo si ambas caben en la misma «ventana» "
  "de 5 valores seguidos. Contando ventanas:")
T([["Mano","Ventanas que contienen ambas cartas","Escalera o mejor (offsuit)"],
   ["JT, T9, 98, 54 (conectadas)","4","≈ 13,4 – 13,5 %"],
   ["Q9, J8 (un hueco de 2)","2","≈ 10,0 – 10,4 %"],
   ["32","2 (A-5 y 2-6)","9,33 %"],
   ["A5","1 (A-2-3-4-5)","8,75 %"],
   ["AK","1 (T-J-Q-K-A)","7,62 %"],
   ["72, K2","0","7,00 % / 6,29 %"]],
  [5.6*cm,5.4*cm,4.4*cm])
P("Más ventanas = más formas de completar una escalera. Las manos con 0 ventanas aún llegan a escalera (≈ 6-7 %) usando una sola carta o con la escalera "
  "entera en la mesa. Además, A-K está en el borde: solo puede formar la escalera más alta. Por eso T9s o 98s tienen más potencial de escalera que AKs.")

# ===== 7
P("7. Validación: cómo sabemos que no hay errores",h1)
P("La prueba más fuerte: si se promedian las 169 manos ponderando por cuántos combos tiene cada una (6, 4 o 12), el resultado debe ser la frecuencia de "
  "cada categoría entre <b>todas</b> las manos de 7 cartas, que es un resultado clásico y conocido.")
P("¿Por qué? Cada conjunto de 7 cartas se puede partir en «2 privadas + 5 de mesa» de C(7,2) = 21 formas. Así que sumar, sobre las 1.326 manos iniciales, los "
  "conteos de cada categoría da exactamente 21 veces el número de manos de 7 cartas de esa categoría. El programa lo comprobó con números enteros:")
known=[23294460,58627800,31433400,6461620,6180020,4047644,3473184,224848,41584]
rows=[["Categoría","Suma ponderada / 21 (este cálculo)","Conteo clásico de 7 cartas","Frecuencia"]]
for i,c in enumerate(cats):
    s=sum((6 if t=="par" else 4 if t=="suited" else 12)*d[k][1+i] for k,t in order)//21
    rows.append([c,fmt(s),fmt(known[i]),f"{known[i]/133784560*100:.4f} %"])
rows.append(["<b>Total</b>","<b>133.784.560</b>","<b>133.784.560</b>","<b>100 %</b>"])
T(rows,[4*cm,4.6*cm,4.2*cm,2.6*cm])
P("Las nueve categorías coinciden <b>exactamente</b>. Un error en la detección de escaleras, colores o fulls haría fallar al menos una fila. Además se "
  "comprobó que en cada mano las categorías suman 2.118.760, y el cálculo a mano del color (sección 6.1) coincide con el programa.")

# ===== 8
S.append(PageBreak())
P("8. Resultados de las 169 manos",h1)
P("Los mapas muestran la probabilidad de terminar en cada nivel al llegar al river. Verde = más potencial, rojo = menos.")
S.append(Image("h_esc.png",width=15*cm,height=13.75*cm))
P("<b>Escalera o mejor:</b> el máximo (17,5 %) lo comparten las conectadas suited de 54s a JTs. AKs se queda en 12,0 %, A5s en 13,1 %. El mínimo está en "
  "manos como K2o (6,3 %). Las parejas van de 12,6 % (AA, KK, 22) a 13,7 % (TT, 55): las de valores medios caben en más escaleras.")
S.append(Image("h_col.png",width=15*cm,height=13.75*cm))
P("<b>Color o mejor:</b> el palo manda. Todas las suited tienen 8,9 %, todas las offsuit 4,3 %. Las parejas marcan 11,4 %, pero no por el color: su color es "
  "bajo (1,96 %, igual que una offsuit) y lo que sube la cifra es el full house (8,55 % frente a 2,22 % de una mano sin pareja) y el póker.")
S.append(Image("h_trio.png",width=15*cm,height=13.75*cm))
P("<b>Trío o mejor:</b> las parejas lideran con 24,4 %: ya tienen dos cartas iguales y solo necesitan una más de las dos que quedan de su valor.")
P("8.1 Desglose completo de algunas manos",h2)
rows=[["Mano"]+["Alta","Par","Doble","Trío","Esc.","Color","Full","Póker","E.col."]]
for h in ["AA","KK","TT","22","AKs","AKo","T9s","98s","98o","A5s","72o"]:
    rows.append([h]+[f"{x/2118760*100:.2f}" for x in d[h][1:10]])
T(rows,[1.3*cm]+[1.57*cm]*9)
P("(Valores en %. Cada fila suma 100.) Observaciones: AA, KK y 22 tienen exactamente el mismo desglose, pero <b>no todas las parejas son iguales</b>: en el color "
  "sí coinciden (todas 11,36 % de color o mejor, porque el palo no depende del valor), pero en escaleras las parejas medias ganan. TT y 55 llegan a escalera o "
  "mejor el 13,7 % de las veces frente al 12,6 % de AA, porque un 10 o un 5 forma parte de 5 de las 10 escaleras posibles, mientras que un As, una K o un 2 solo de 2. "
  "Aun así, AA es muy superior contra un rival: ahí gana la categoría más alta y, dentro de ella, el valor más alto.")

# ===== 9
S.append(PageBreak())
P("9. Potencial frente a equity",h1)
P("<b>Potencial</b> responde: ¿qué tan a menudo tu mano llega a cada categoría? <b>Equity</b> responde: ¿qué parte del bote te corresponde en promedio contra "
  "un rival? Son cosas distintas. 32o arma escalera más veces que AKo (9,3 % frente a 7,6 %), pero pierde muchísimo más, porque sus parejas y escaleras "
  "suelen ser más bajas que las del rival.")
P("<b>Definición de equity:</b>",h2)
P("Equity = [victorias + Σ (1/k por cada empate entre k jugadores)] / escenarios",eq)
P("Sobre los empates: un empate solo ocurre cuando las 5 mejores cartas de ambos jugadores tienen exactamente el mismo valor. Los desempates por carta "
  "acompañante (kicker) ya se resuelven antes: si tu kicker es mayor, es victoria, no empate. En un empate real el bote se parte, así que te llevas 1/k "
  "de él. No es una aproximación: es lo que cobras.")
P("9.1 Equity de A-A contra rivales con manos aleatorias",h2)
T([["Rivales","1","2","3","5","8"],["Equity de A♠A♥","85,4 %","73,8 %","63,9 %","49,5 %","34,2 %"]],[3.6*cm]+[2.3*cm]*5)
P("Método: simulación Monte Carlo con 40.000 manos por celda (error aprox. ± 0,3 %). Con más rivales la equity cae porque el bote se reparte entre más "
  "manos que pueden mejorar. Estas cifras suponen que los rivales llegan al showdown con cualquier mano; en la práctica, quien paga un all-in tiene manos "
  "más fuertes y la equity real es menor.")
P("9.2 El efecto de las cartas que ves (blockers)",h2)
P("Si no ves las cartas del rival, no cambian tus probabilidades: para ti son tan desconocidas como las del mazo. Pero en un all-in preflop las cartas se "
  "voltean antes de repartir la mesa, y entonces esas cartas salen del mazo y sí cambian el cálculo. Resultados con 9♣8♣ contra una mano revelada, por "
  "enumeración exacta de las 1.712.304 mesas posibles (C(48,5)):")
T([["Rival muestra","Escalera o mejor","Color o mejor","Tu equity","Por qué"],
   ["(nada visto)","17,46 %","8,93 %","≈ 50,7 % vs aleatoria","Referencia"],
   ["A♥A♠","19,35 %","9,90 %","22,6 %","Salen dos cartas inútiles para ti: tu potencial sube, pero los ases ganan"],
   ["A♣K♣","16,35 %","6,68 %","34,8 %","Te quita 2 tréboles y una K de tus escaleras"],
   ["7♥7♦","16,07 %","9,90 %","49,1 %","Los sietes completan tus escaleras: te los bloquea"],
   ["7♣6♣","14,22 %","6,68 %","66,5 %","Te quita tréboles y cartas de escalera, pero sus cartas son más bajas"]],
  [2.4*cm,2.5*cm,2.3*cm,2.9*cm,5.3*cm])
P("Tus propias cartas funcionan igual: con A-A en la mano, un rival solo puede tener A-A de 1 forma entre C(50,2) = 1.225 (0,08 %), en lugar de 6 entre 1.326 "
  "(0,45 %).")

# ===== 10
P("10. Qué NO mide este análisis y siguientes pasos",h1)
P("• <b>No es probabilidad de ganar.</b> Hace falta la equity contra el rango del rival.<br/>"
  "• <b>No considera rivales.</b> Supone que no ves ninguna otra carta. Con cartas reveladas los números cambian (sección 9.2).<br/>"
  "• <b>La columna «Mejora al tablero» es aproximada.</b> Detecta cuándo tus cartas suben la categoría de la mesa, pero no mejoras dentro de la misma categoría "
  "(por ejemplo, un color más alto que el de la mesa). En las parejas marca ≈ 94,7 % porque la pareja en mano casi siempre sube la categoría de la mesa.<br/>"
  "• <b>No distingue la fuerza dentro de una categoría.</b> Una escalera 5-9 y una T-A cuentan igual aquí.<br/>"
  "• <b>No incluye apuestas, posición, tamaño de las fichas ni comportamiento del rival.</b>")
P("<b>Siguientes pasos naturales:</b> (1) tabla de equity 169 × 169 cara a cara, por enumeración exacta; (2) equity contra 1 a 8 rivales y contra rangos de "
  "pago (top 5 %, 10 %, 20 %…); (3) versión del potencial que exija usar al menos una carta propia; (4) capa de decisión para el all-in preflop (bote, "
  "fichas efectivas y probabilidad de que los rivales se retiren).")

# ===== Anexo A
S.append(PageBreak())
P("Anexo A. Tabla completa de las 169 manos",h1)
P("Probabilidad (%) al river, por enumeración exacta de 2.118.760 mesas por mano. «Doble+» incluye parejas y dobles parejas que ya vienen en la mesa. "
  "Orden: parejas, luego por carta alta, suited antes que offsuit.")
rows=[["Mano","Combos","Pareja+","Doble+","Trío+","Esc.+","Color+","Full+"]]
def key_sort(k):
    R="AKQJT98765432"
    if len(k)==2: return (0,R.index(k[0]),0)
    return (1,R.index(k[0]),R.index(k[1]),0 if k[2]=="s" else 1)
for k in sorted(d,key=key_sort):
    v=d[k]; n=v[0]; c=v[1:10]
    cmb=6 if len(k)==2 else (4 if k[2]=="s" else 12)
    rows.append([k,cmb]+[f"{sum(c[i:])/n*100:.2f}" for i in (1,2,3,4,5,6)])
T(rows,[1.9*cm,1.9*cm]+[1.95*cm]*6)

# ===== Anexo B
S.append(PageBreak())
P("Anexo B. Código fuente del cálculo",h1)
P("Programa en C usado para producir todos los conteos. Compilar con <font name='DVM'>gcc -O2 -o pot pot.c</font> y ejecutar <font name='DVM'>./pot &gt; raw.csv</font>. "
  "Cada línea de salida es: mano, tipo, número de mesas, los 9 conteos por categoría y el conteo de «mejora a la mesa».")
code(open("pot.c").read())

def deco(c,doc):
    c.saveState(); c.setFont("DV",7.5); c.setFillColor(colors.grey)
    c.drawString(2*cm,1.2*cm,"Probabilidades preflop en Texas Hold'em — potencial de las 169 manos")
    c.drawRightString(A4[0]-2*cm,1.2*cm,f"Página {doc.page}"); c.restoreState()
doc=SimpleDocTemplate("/mnt/user-data/outputs/probabilidades_preflop_holdem.pdf",pagesize=A4,
    leftMargin=2*cm,rightMargin=2*cm,topMargin=1.8*cm,bottomMargin=1.9*cm,
    title="Probabilidades preflop en Texas Hold'em",author="Claude")
doc.build(S,onFirstPage=deco,onLaterPages=deco)
print("ok")
