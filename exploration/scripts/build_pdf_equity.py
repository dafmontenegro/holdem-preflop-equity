import csv
from reportlab.lib.pagesizes import A4
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle, PageBreak, KeepTogether
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib import colors
from reportlab.lib.units import cm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfbase.pdfmetrics import registerFontFamily
F="/usr/share/fonts/truetype/dejavu/"
pdfmetrics.registerFont(TTFont("DV",F+"DejaVuSans.ttf")); pdfmetrics.registerFont(TTFont("DVB",F+"DejaVuSans-Bold.ttf"))
pdfmetrics.registerFont(TTFont("DVI",F+"DejaVuSans-Oblique.ttf"))
registerFontFamily("DV",normal="DV",bold="DVB",italic="DVI",boldItalic="DVB")
NAVY=colors.HexColor("#1F3864")
body=ParagraphStyle("b",fontName="DV",fontSize=9.6,leading=14,spaceAfter=6,alignment=4)
h1=ParagraphStyle("h1",fontName="DVB",fontSize=15,leading=19,textColor=NAVY,spaceBefore=8,spaceAfter=8)
h2=ParagraphStyle("h2",fontName="DVB",fontSize=11.5,leading=15,textColor=NAVY,spaceBefore=6,spaceAfter=5)
title=ParagraphStyle("t",fontName="DVB",fontSize=22,leading=27,textColor=NAVY,spaceAfter=8)
sub=ParagraphStyle("s",fontName="DV",fontSize=11,leading=15,textColor=colors.HexColor("#444444"),spaceAfter=10)
box=ParagraphStyle("box",parent=body,backColor=colors.HexColor("#EEF3FA"),borderColor=colors.HexColor("#9DB5D8"),borderWidth=0.6,borderPadding=7,spaceBefore=6,spaceAfter=12)
warn=ParagraphStyle("w",parent=box,backColor=colors.HexColor("#FFF4E0"),borderColor=colors.HexColor("#E0A84A"))
eqs=ParagraphStyle("eq",fontName="DV",fontSize=11,leading=16,alignment=1,spaceBefore=2,spaceAfter=8)
cell=ParagraphStyle("c",fontName="DV",fontSize=8.4,leading=10.5)
cellb=ParagraphStyle("cb",fontName="DVB",fontSize=8.4,leading=10.5,textColor=colors.white)
CF,CJ,CM,CD=colors.HexColor("#2E7D32"),colors.HexColor("#C8E6C9"),colors.HexColor("#FFE0A3"),colors.HexColor("#F6C6C6")
E={}
for r in csv.reader(open("eq8.csv")): E[r[0]]=[float(x)*100 for x in r[1:]]
R="AKQJT98765432"
def key(i,j): return R[i]+R[i] if i==j else (R[i]+R[j]+"s" if i<j else R[j]+R[i]+"o")
fair=lambda n:100/(n+1)
def cls(k,n):
    r=E[k][n-1]/fair(n); return "f" if r>=1.4 else "j" if r>=1.1 else "m" if r>=0.95 else "d"
COL={"f":CF,"j":CJ,"m":CM,"d":CD}
S=[]; P=lambda t,st=body:S.append(Paragraph(t,st))
def T(rows,widths,extra=[]):
    data=[[Paragraph(str(x),cellb if i==0 else cell) for x in r] for i,r in enumerate(rows)]
    t=Table(data,colWidths=widths,repeatRows=1)
    t.setStyle(TableStyle([("GRID",(0,0),(-1,-1),0.4,colors.HexColor("#B0B7C3")),("BACKGROUND",(0,0),(-1,0),NAVY),
        ("VALIGN",(0,0),(-1,-1),"MIDDLE"),("TOPPADDING",(0,0),(-1,-1),2.5),("BOTTOMPADDING",(0,0),(-1,-1),2.5)]+extra))
    S.append(t); S.append(Spacer(1,8))
def legend():
    t=Table([["Fuerte (≥ 1,4×)","Jugable (1,1–1,4×)","Marginal (0,95–1,1×)","Retirarse (< 0,95×)"]],colWidths=[4.2*cm]*4)
    t.setStyle(TableStyle([("FONTNAME",(0,0),(-1,-1),"DV"),("FONTSIZE",(0,0),(-1,-1),8.5),("ALIGN",(0,0),(-1,-1),"CENTER"),
        ("BACKGROUND",(0,0),(0,0),CF),("TEXTCOLOR",(0,0),(0,0),colors.white),("BACKGROUND",(1,0),(1,0),CJ),
        ("BACKGROUND",(2,0),(2,0),CM),("BACKGROUND",(3,0),(3,0),CD),("GRID",(0,0),(-1,-1),0.4,colors.white)]))
    S.append(t); S.append(Spacer(1,8))
def grid(n):
    data=[[""]+list(R)]; sty=[("FONTNAME",(0,0),(-1,-1),"DV"),("FONTSIZE",(0,0),(-1,-1),6.6),("LEADING",(0,0),(-1,-1),7.6),
        ("ALIGN",(0,0),(-1,-1),"CENTER"),("VALIGN",(0,0),(-1,-1),"MIDDLE"),("GRID",(0,0),(-1,-1),0.6,colors.white),
        ("BACKGROUND",(0,0),(-1,0),NAVY),("BACKGROUND",(0,0),(0,-1),NAVY),("TEXTCOLOR",(0,0),(-1,0),colors.white),
        ("TEXTCOLOR",(0,0),(0,-1),colors.white),("FONTNAME",(0,0),(-1,0),"DVB"),("FONTNAME",(0,0),(0,-1),"DVB"),
        ("TOPPADDING",(0,0),(-1,-1),1.2),("BOTTOMPADDING",(0,0),(-1,-1),1.2)]
    for i in range(13):
        row=[R[i]]
        for j in range(13):
            k=key(i,j); c=cls(k,n); row.append(f"{k}\n{E[k][n-1]:.1f}")
            sty.append(("BACKGROUND",(j+1,i+1),(j+1,i+1),COL[c]))
            if c=="f": sty.append(("TEXTCOLOR",(j+1,i+1),(j+1,i+1),colors.white))
        data.append(row)
    t=Table(data,colWidths=[0.6*cm]+[1.25*cm]*13,rowHeights=[0.45*cm]+[0.62*cm]*13)
    t.setStyle(TableStyle(sty)); return t

# ===== Intro
P("Equity preflop de las 169 manos, de 2 a 9 jugadores",title)
P("Probabilidad de ganar el bote con cada mano inicial contra 1 a 8 rivales, y cómo usarla para decidir si jugar",sub)
P("1. Qué muestran las tablas",h1)
P("Cada número es la <b>equity</b> de una mano: el porcentaje del bote que ganas en promedio si todos los jugadores llegan al final (al river) sin "
  "retirarse. Los empates cuentan como la fracción del bote que te toca (la mitad si empatan dos, un tercio si empatan tres). Ejemplo: A-A contra 1 "
  "rival tiene 85,3 %: de cada 100 veces que se enfrenta a una mano cualquiera, gana el equivalente a 85 botes.")
P("Las tablas suponen que <b>los rivales tienen manos al azar</b> (cualquier par de cartas, sin filtrar). Esto sirve para saber qué manos son buenas en "
  "general; más abajo se explica por qué cambia en un all-in.")
P("2. Cómo encontrar tu mano en la cuadrícula",h1)
P("• La <b>carta más alta</b> marca la <b>fila</b> y la <b>más baja</b> la <b>columna</b>.<br/>"
  "• <b>Mismo palo (suited, «s»):</b> la casilla está <b>arriba</b> de la diagonal.<br/>"
  "• <b>Palos distintos (offsuit, «o»):</b> la casilla está <b>abajo</b> de la diagonal.<br/>"
  "• <b>Pareja:</b> está <b>en la diagonal</b>, donde fila y columna coinciden.")
P("Ejemplos: K♠7♠ → fila K, columna 7, arriba (K7s). K♠7♥ → fila K, columna 7, abajo (K7o). 7♣7♦ → diagonal, en la fila 7 (77). "
  "La «T» es el 10.")
P("3. La regla para decidir: tu parte justa",h1)
P("Si hay <i>J</i> jugadores y nadie tiene ventaja, a cada uno le corresponde 1/<i>J</i> del bote. Esa es tu <b>parte justa</b>. La pregunta es cuántas "
  "veces tu parte justa gana tu mano:")
P("Índice = equity de tu mano / parte justa      (parte justa = 100 % / número de jugadores)",eqs)
T([["Jugadores","2","3","4","5","6","7","8","9"],["Rivales","1","2","3","4","5","6","7","8"],
   ["Parte justa"]+[f"{fair(n):.1f} %".replace(".",",") for n in range(1,9)]],[2.6*cm]+[1.6*cm]*8)
P("Un índice mayor que 1 significa que tu mano gana más de lo que le corresponde. Los colores de todas las tablas usan este índice:")
legend()
P("<b>Cómo usarlo:</b> fuerte y jugable → vale la pena jugar. Marginal → depende de la posición, del precio y de los rivales. Retirarse → a la larga "
  "pierdes dinero si juegas esa mano.",box)
P("4. Ejemplos resueltos",h1)
ex=[("7♥2♣","72o",1),("9♣8♣","98s",1),("9♣8♣","98s",5),("2♠2♥","22",1),("2♠2♥","22",8),("A♦5♠","A5o",1),("A♦5♠","A5o",5),("K♣J♦","KJo",3),("A♠A♥","AA",8)]
names={"f":"Fuerte","j":"Jugable","m":"Marginal","d":"Retirarse"}
rows=[["Mano","Casilla","Jugadores","Equity","Parte justa","Índice","Decisión"]]
extra=[]
for i,(m,k,n) in enumerate(ex,1):
    e=E[k][n-1]; c=cls(k,n)
    rows.append([m,k,n+1,f"{e:.1f} %".replace(".",","),f"{fair(n):.1f} %".replace(".",","),f"{e/fair(n):.2f}".replace(".",","),names[c]])
    extra.append(("BACKGROUND",(6,i),(6,i),COL[c]))
    if c=="f": extra.append(("TEXTCOLOR",(6,i),(6,i),colors.white))
T(rows,[1.9*cm,1.7*cm,2*cm,2*cm,2.3*cm,1.8*cm,2.6*cm],extra)
P("Lo que muestran: 9♣8♣ es una moneda al aire contra 1 rival pero jugable en mesa llena, porque sus escaleras y colores ganan botes con muchos "
  "jugadores. Las parejas pequeñas mejoran con más rivales (buscan trío). Los ases con carta baja, como A5o, van al revés: buenos contra 1, flojos contra 5.")
P("5. Advertencia para el all-in preflop",h1)
P("Si vas all-in, <b>los rivales que pagan no tienen manos al azar</b>: pagan con manos fuertes (parejas altas, A-K, A-Q…). Contra ese rango tu equity es "
  "mucho menor que la de estas tablas. Ejemplo: 9♣8♣ tiene ~51 % contra una mano cualquiera, pero ~23 % contra A-A. Por eso estas tablas responden "
  "«¿vale la pena jugar esta mano?», y no «¿vale la pena ir all-in?». Para lo segundo hace falta la equity contra el rango del que paga y la "
  "probabilidad de que todos se retiren.",warn)
P("6. Método y precisión",h1)
P("Simulación Monte Carlo en C: para cada mano y cada número de rivales se repartieron <b>100.000 manos completas</b> (cartas de los rivales y 5 cartas "
  "de mesa, al azar y sin reemplazo), evaluando la mejor mano de 5 de 7 cartas de cada jugador con desempate completo por kickers. "
  "Error estándar: como máximo ±0,16 puntos porcentuales (intervalo de 95 % ≈ ±0,3). "
  "Validación: el promedio de las 169 manos ponderado por combos (6 parejas, 4 suited, 12 offsuit) debe dar exactamente la parte justa. Resultado:")
chk=[["Rivales"]+[str(n) for n in range(1,9)],["Promedio medido"],["Parte justa"]]
w=lambda k:6 if len(k)==2 else (4 if k[2]=="s" else 12)
for n in range(1,9):
    chk[1].append(f"{sum(w(k)*E[k][n-1] for k in E)/1326:.2f} %".replace(".",","))
    chk[2].append(f"{fair(n):.2f} %".replace(".",","))
T(chk,[3*cm]+[1.6*cm]*8)
P("Coinciden en todos los casos (diferencias de 0,02 puntos o menos). También coinciden con valores publicados: A-A ≈ 85 % y 7-2 offsuit ≈ 35 % contra 1 rival.")

# ===== Grids
S.append(PageBreak())
P("7. Cuadrículas por número de jugadores",h1)
P("Cada casilla: mano y equity (%). Arriba de la diagonal suited, diagonal parejas, abajo offsuit.")
for n in range(1,9):
    blk=[Paragraph(f"{n+1} jugadores ({n} {'rival' if n==1 else 'rivales'}) — parte justa {fair(n):.1f} %".replace(".",","),h2)]
    if n==1: pass
    blk.append(grid(n)); blk.append(Spacer(1,10))
    S.append(KeepTogether(blk))
    if n==1: 
        S.append(PageBreak())
    elif n%2==1: S.append(PageBreak())
# ===== Resumen conteo
P("8. ¿Cuántas manos vale la pena jugar?",h1)
P("Porcentaje de las 1.326 combinaciones de cartas que caen en cada categoría. Con más jugadores hay que ser más selectivo.")
rows=[["Jugadores","Fuerte","Jugable","Marginal","Retirarse","Jugar (fuerte + jugable)"]]
for n in range(1,9):
    cnt={"f":0,"j":0,"m":0,"d":0}
    for k in E: cnt[cls(k,n)]+=w(k)
    rows.append([n+1]+[f"{cnt[c]/1326*100:.1f} %".replace(".",",") for c in "fjmd"]+[f"{(cnt['f']+cnt['j'])/1326*100:.1f} %".replace(".",",")])
T(rows,[2.4*cm,2.3*cm,2.3*cm,2.3*cm,2.3*cm,4.4*cm])
# ===== Tabla completa
S.append(PageBreak())
P("9. Tabla completa: equity (%) de las 169 manos",h1)
P("Columnas = número de jugadores. El color de cada celda sigue la regla de la parte justa. Orden: parejas, luego por carta alta; suited antes que offsuit.")
def ks(k):
    if len(k)==2: return (0,R.index(k[0]),0,0)
    return (1,R.index(k[0]),R.index(k[1]),0 if k[2]=="s" else 1)
rows=[["Mano","Combos"]+[f"{n+1} jug." for n in range(1,9)]]; extra=[]
for i,k in enumerate(sorted(E,key=ks),1):
    rows.append([k,w(k)]+[f"{E[k][n-1]:.1f}" for n in range(1,9)])
    for n in range(1,9):
        c=cls(k,n); extra.append(("BACKGROUND",(n+1,i),(n+1,i),COL[c]))
        if c=="f": extra.append(("TEXTCOLOR",(n+1,i),(n+1,i),colors.white))
data=[[Paragraph(str(x),cellb if i==0 else ParagraphStyle("x",parent=cell,fontSize=7.8,leading=9)) for x in r] for i,r in enumerate(rows)]
t=Table(data,colWidths=[1.6*cm,1.9*cm]+[1.62*cm]*8,repeatRows=1)
t.setStyle(TableStyle([("GRID",(0,0),(-1,-1),0.4,colors.white),("BACKGROUND",(0,0),(-1,0),NAVY),("VALIGN",(0,0),(-1,-1),"MIDDLE"),
    ("TOPPADDING",(0,0),(-1,-1),1.3),("BOTTOMPADDING",(0,0),(-1,-1),1.3)]+extra))
# paragraphs ignore TEXTCOLOR; recolor white text for 'f'
for i,k in enumerate(sorted(E,key=ks),1):
    for n in range(1,9):
        if cls(k,n)=="f": data[i][n+1]=Paragraph(f"{E[k][n-1]:.1f}",ParagraphStyle("xw",parent=cell,fontSize=7.8,leading=9,textColor=colors.white))
t=Table(data,colWidths=[1.6*cm,1.9*cm]+[1.62*cm]*8,repeatRows=1)
t.setStyle(TableStyle([("GRID",(0,0),(-1,-1),0.4,colors.white),("BACKGROUND",(0,0),(-1,0),NAVY),("VALIGN",(0,0),(-1,-1),"MIDDLE"),
    ("TOPPADDING",(0,0),(-1,-1),1.3),("BOTTOMPADDING",(0,0),(-1,-1),1.3)]+extra))
S.append(t)
def deco(c,doc):
    c.saveState(); c.setFont("DV",7.5); c.setFillColor(colors.grey)
    c.drawString(2*cm,1.2*cm,"Equity preflop de las 169 manos, de 2 a 9 jugadores"); c.drawRightString(A4[0]-2*cm,1.2*cm,f"Página {doc.page}"); c.restoreState()
doc=SimpleDocTemplate("/mnt/user-data/outputs/equity_preflop_2_a_9_jugadores.pdf",pagesize=A4,leftMargin=1.8*cm,rightMargin=1.8*cm,
    topMargin=1.6*cm,bottomMargin=1.8*cm,title="Equity preflop de las 169 manos, de 2 a 9 jugadores",author="Claude")
doc.build(S,onFirstPage=deco,onLaterPages=deco); print("ok")
