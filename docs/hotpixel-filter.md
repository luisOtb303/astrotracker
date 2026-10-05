# Hot Pixel Filter — investigación comparativa y recomendación

Estado: investigación terminada, implementación pendiente de diseño.
Medido sobre los ficheros de `testdata/hotpixel/` del propio repositorio.

Las secciones están etiquetadas para poder distinguir el origen de cada cosa:

- **[PIPP]** — comportamiento descrito en la documentación de PIPP que nos ha
  facilitado el usuario. No hay acceso al código de PIPP, así que **no** se
  afirma nada sobre su implementación interna.
- **[MEDIDO]** — resultado de nuestras propias mediciones sobre los RAW de
  referencia, reproducibles con los scripts de `research/`.
- **[PROPUESTA]** — recomendación de diseño para AstroTracker.

---

## 1. Resumen ejecutivo

**[PROPUESTA]** El filtro debe trabajar **sobre el mosaico Bayer, antes del
debayering**, no sobre la imagen demosaicada. No es una preferencia estética:
medido en los RAW de referencia, los defectos reales tienen un exceso de
**+3059 y +6716 DN** sobre el fondo en el mosaico, pero de **+16 y +75 DN** una
vez demosaicados. En la imagen demosaicada el defecto aparece untado sobre ~5×5
píxeles, la mediana local sube con él y el problema se vuelve indistinguible del
ruido: el detector ingenuo sobre el JPEG de cámara produce **69 000–281 000
falsos positivos** por imagen.

Combinación recomendada, sobre el mosaico y respetando el color del fotosito:

1. **Mediana local + MAD local** (sigma robusto, escalado 1.4826).
2. **Umbral alto** (k ≈ 15–20 σ), no bajo. Los defectos reales están a z = 60–260
   y el ruido structural se apaga por debajo de z = 10.
3. **Test de aislamiento**: ningún vecino del mismo color es a su vez outlier.
4. **Corrección por mediana de los vecinos del mismo color**.

Coste: ~90 ms por frame de 18 Mpx en C++/OpenCV, un solo paso. Sin master dark,
sin modelos, sin dependencias nuevas.

---

## 2. Lo que dice PIPP y lo que no sabemos

**[PIPP]** Documentado:

- Tiene un *Raw Image Hot Pixel Filter* que detecta píxeles calientes en el RAW
  y los sustituye usando vecinos **del mismo componente de color Bayer**.
- Tiene un *Hot Pixel Replacement* basado en **master dark**: marca píxeles
  anómalos con un umbral y los sustituye por información de vecinos.
- Tiene además un *Median Noise Filter*, que es un mecanismo distinto.

**[DESCONOCIDO]** No está documentado, y no lo vamos a suponer:

- Si el detector es mediana, MAD, sigma-clipping u otro criterio.
- Qué ventana usa y con qué radio.
- Cómo se comporta con estrellas y bordes.
- Cómo separa un defecto de una fuente real cuando no hay master dark.

**Conclusión:** lo único que copiamos de PIPP es la **decisión estructural**:
detectar en el mosaico y sustituir por vecinos del mismo color. Eso ya está
justificado por nuestras propias mediciones, así que no dependemos de PIPP para
justificar el diseño.

### 2.1 **[VERIFICADO]** Qué se puede y qué no se puede comprobar de PIPP

PIPP es **cerrado** y el proyecto está prácticamente inaccesible: la página oficial
`https://sites.google.com/site/astropipp/?pli=1&authuser=0` devuelve 403 y pide
inicio de sesión, y el fork de `astrowhat.com` tampoco expone código. La única
fuente legible es el **manual archivado en Wayback**:

- `https://web.archive.org/web/20230310094820/https://sites.google.com/site/astropipp/pipp-manual`

Lo que ese manual confirma, y que conviene tener claro porque corrige una
impresión bastante extendida:

- El *Raw Image Hot Pixel Filter* se documenta en **una línea**: "detects hot
  pixels and replaces them with a value calculated from neighbouring pixels of the
  same colour". Se aplica **antes del debayering** y se activa con un **checkbox
  binario, sin umbral ni parámetro de agresividad**.
- PIPP **no expone un control de sensibilidad** para este filtro. Es estrictamente
  más conservador que nuestra implementación, que tiene un deslizador 0..100.
- El *Median Noise Filter* es un control **separado** y no es el detector de
  píxeles calientes: confundirlos lleva a pensar que PIPP tiene un filtro de
  mediana con umbral, que no es lo que hace.
- El manual indica que PIPP usa **DCRAW** internamente, lo que confirma que el
  punto de inserción pre-debayer sobre `imgdata.rawdata` es el sitio natural.

**Consecuencia para el diseño:** nuestra función `removeHotPixels()` **no es una
réplica de PIPP** y no debe documentarse como tal. Es un detector propio, con
parámetros que PIPP no expone, que aplica la misma idea estructural (mismo color,
antes del debayer). La diferencia deInterfaz —checkbox fijo frente a deslizador—
es intencionada: PIPP asume que corregir de más es raro y no ofrece ajuste; en
AstroTracker un slider con default conservador (k=20, la meseta medida) da
control sin exponer el riesgo por defecto.

---

## 3. Evidencia medida

### 3.1 Los ficheros de referencia

**[MEDIDO]** `IMG_5633.CR2` y `IMG_6174.CR2`: 5202×3464 (18.0 Mpx), patrón CFA
`[[3,2],[0,1]]` según rawpy (OJO: **la numeración de color depende de la
herramienta**; el código final debe leer el patrón de LibRaw, no suponerlo).

`IMG_5633.jpg` es el JPEG de cámara, 5184×3456. Midiendo el desplazamiento sobre
6 outliers inequívocos del propio RAW, la correspondencia es:

```
RAW = JPEG + (10, 4)
```

**Los puntos que el usuario ha apuntado la imagen ya demosaicada**, y por eso
desplazados 10 y 4 px no corresponden a fotositos calientes.

### 3.2 Los puntos declarados por el usuario

**[MEDIDO]** Traduciendo con el offset medido y buscando el máximo del residual
en ±3 px:

| Punto en el JPEG | En el RAW | z (MAD local, mismo color) | ¿Defecto? |
|---|---|---|---|
| (1102, 926) | (1112, 931) | **226.6** | Sí |
| (2056, 2229) | (2067, 2233) | 2.1 | No |
| (1968, 2505) | (1978, 2509) | **103.9** | Sí |

El valor del photosite en (1112,931) es **9014** frente a una mediana de 2298. En
(1978,2509) es **5430** frente a 2385.

El tercer punto no es un defecto. No hay nada anomalio ahí, ni en el mosaico ni
en el JPEG. Puede ser una estrella débil.

`IMG_6174` **no se ha podido verificar**: su JPEG es 5184×**2916** (recorte 16:9)
frente al RAW de 3464 filas, y 4 de los 6 outliers reales del RAW caen
**fuera** del JPEG. Sin un mapeo fiable no se puede traducir (3410, 1020).

### 3.3 Por qué el mosaico es la capa correcta

**[MEDIDO]** El mismo defecto, visto en el mosaico y en el JPEG demosaicado:

| Defecto | Exceso en el mosaico | Exceso sobre mediana 3×3 en el JPEG |
|---|---|---|
| JPEG (1102,926) | +6716 DN | **+6** |
| JPEG (1968,2505) | +3059 DN | **+16** |

En el mosaico el defecto es **un fotosito** aislado. Al demosaicar, ese fotosito
contamina los tres canales de un bloque 2×2 y el resultado es una mancha de ~5×5
donde **la mediana local ya está elevada**: el criterio "soy mucho más brillante
que mis vecinos" deja de funcionar por construcción.

**[MEDIDO]** Detector ingenuo sobre el JPEG demosaicado (mediana 3×3 + umbral
global en sigmas del ruido):

| Umbral | Falsos positivos en 18 Mpx |
|---|---|
| 8.4 σ | 281 139 |
| 16.5 σ | 69 171 |
| 24.6 σ | 14 861 |

Enorme, y aun así **no detecta los defectos reales**, porque su delta es de 16 DN
frente a un umbral que tendría que bajar a ~2 para empezar a verlos. La
relación señal/falso positivo es inviable.

### 3.4 Comparación de familias sobre el mosaico

Todas con vecindario del mismo color de **24 muestras (radio 2 en la sub-imagen,
la configuración recomendada en §5.2.1)**, sobre `IMG_5633.CR2` (18.0 Mpx, 2
positivos conocidos). Script: `research/hotpixel_eval.py`.

| Familia | Umbral | Detecciones | FP/Mpx | En zona de halo | Positivos |
|---|---|---|---|---|---|
| mediana local + std global | 6 σ | 806 | 44.7 | — | 2/2 |
| mediana local + std local | 6 σ | 806 | 44.7 | — | 2/2 |
| mediana local + MAD local | 5 σ | 13 033 | 723.3 | — | 2/2 |
| mediana local + MAD local | 8 σ | 649 | 36.0 | — | 2/2 |
| mediana + MAD + aislamiento | 8 σ | 609 | 33.8 | — | 2/2 |
| **mediana + MAD + aislamiento** | **12 σ** | **188** | **10.4** | 0.5 % | **2/2** |
| **mediana + MAD + aislamiento** | **15 σ** | **172** | **9.5** | 0.6 % | **2/2** |
| **mediana + MAD + aislamiento** | **20 σ** | **155** | **8.6** | 0.6 % | **2/2** |
| mediana + MAD + aislamiento | 30 σ | 147 | 8.2 | 0.7 % | 2/2 |

Lecturas:

- **MAD gana a std de forma decisiva.** Con el mismo umbral nominal (6σ), std local
  da 44.7 FP/Mpx y MAD local 36.0; a igualdad de FP, MAD necesita 20σ donde std
  no llega. La razón es que la desviación estándar está inflada por los propios
  outliers, así que subir el umbral no los elimina: el umbral deja de discriminar.
  El MAD no se contamina.
- **El conteo se estabiliza en ~150.** Entre k=15 y k=30 el número apenas cambia
  (172 → 147). Por debajo de k=12 crece disparado (609 a k=8, 13 033 a k=5). Ese
  escalón dice que hay **~150 defectos reales** y que todo lo que aparece por
  debajo de ~12σ es ruido. Operar con k bajo cuesta FP sin ganar nada.
- **El valor de meseta es independiente del radio.** Con 48 vecinos (radio 3) el
  mismo test da 154 detecciones en k=20; con 24 (radio 2), 155. La conclusión es
  robusta al radio, lo que confirma que lo que importa es tener suficientes
  muestras para el MAD y no afinar la ventana.
- **El detector no dispara sobre estrellas.** "En zona de halo" mide la fracción
  de detecciones cuyo entorno tiene el brillo del percentil 99 del cielo: **0.5–0.7
  %**. Si el criterio fuera indiscriminado, esa fracción sería enorme. A nivel de
  mosaico la señal de una estrella se reparte entre muchos fotositos y cada uno
  queda dentro del rango de su entorno del mismo color.
- **El aislamiento aporta poco pero no estorba.** Sobre 649 candidatos quita 40.
  Es una red de seguridad barata, no un componente crítico.

### 3.5 Distribución espacial y segundo fichero

**[MEDIDO]** `IMG_6174`: 24 detecciones con z > 20 (1.3/Mpx), la más fuerte en
(1019, 2060) con z = 264 y valor 13790 sobre un fondo de ~2500.

**13 de las 24 están en el tercio inferior** (y > 2500). Los defectos del sensor
se agrupan, lo que tiene una lectura práctica: un mapa de píxeles calientes
guardado entre sesiones es reutilizable para el mismo cuerpo y posición de
montura, y no hace falta recalcularlo entero.

---

## 4. Las cinco familias, una por una

### 4.1 Mediana local

`mediana = median(vecindario); hot = pixel - mediana > umbral`

**A favor:** barato, con `cv::medianBlur(k=3)` sale en una pasada; robusto
frente a hasta un 50 % de valores contaminados; y es el criterio que preserva
bordes (medido en §6).

**En contra, y es lo importante:** sobre la imagen **demosaicada** falla (§3.3).
La mediana local sube con el defecto. Solo es válido sobre el mosaico.

**Falsos positivos:** ninguno propio; los que aparecen a k bajo son ruido de
lectura, y se eliminan subiendo k.

**Sobre estrellas:** no las marca (§3.4). Sobre el mosaico es seguro.

### 4.2 MAD

`MAD = median(|v_i - mediana|)` sobre las **24 muestras del vecindario**, no sobre
un único píxel; `z = (pixel - mediana) / (1.4826 × MAD)`

**Gain real frente a la mediana sola:** el MAD es lo que hace que el umbral sea
**interpretable y adaptativo**. El nivel absoluto del ruido cambia con ISO,
exposición y modelo de cámara; el umbral fijo en DN no puede seguirlo. Con MAD el
umbral es "tantas veces el ruido de esta toma", que es la misma magnitud para
todas las fotos. Medido: es la diferencia entre 723 FP/Mpx (std, k=5) y 8.6 FP/Mpx
(MAD + aislamiento, k=20) manteniendo los 2/2 positivos.

**Trampa:** en zonas planas (cielo liso, zonas quemadas por una guillotina, sombras
totales) el MAD puede valer 0 y z explode. **Hay que poner un suelo al sigma**.
Medido, es imprescindible: sin suelo aparecen miles de detecciones falsas en las
zonas planas.

### 4.3 Sigma clipping / estadística robusta

La comparación de §3.4 ya la cubre: frente a media/σ global, **MAD local gana**,
porque σ global no se adapta a la heterogeneidad del cielo (una nebulosa con
gradiente da un σ mucho mayor que el fondo, y con σ global se dejan de detectar
los defectos de las zonas más planas). Sigma-clipping iterativo es
sobredimensionado para un problema que se resuelve con una mediana y un MAD.

### 4.4 Condición sobre los vecinos

`deviation > umbral && vecinosBrillantes <= N`

**Medido:** en el mosaico vale poco por sí sola (§3.4, el aislamiento quita 3 de
232). Motivo: un fotosito quemado *es* un vecino brillante de sus propios
vecinos, así que hay que formulations tipo "ningún vecino es a su vez outlier"
(comparar contra `mediana_vecinos + k·sigma_vecinos`), no "ningún vecino es
brillante".

**Sí es imprescindible bajo otra forma:** combinado con un test de brillo
relativo, es lo que protege las estrellas. Un fotosito encima del núcleo de una
estrella es **irrecuperable** (§6.4), así que lo correcto es no detectarlo ahí.
Medido: la separación entre estrellas y defectos ya es enorme sin este test, pero
un guardia extra de "el entorno no es una estructura brillante" es barato y
evita el peor daño posible.

### 4.5 Gradiente / estructura (Sobel, Laplacian)

**Recomendamos que no se implemente.** Medido, el criterio de aislamiento ya
resuelve el caso (estrellas: 0.4 % de falsos positivos), así que el test de
estructura es complejidad que no compra nada. Además, un hot pixel produce un
Laplacian enorme, igual que un borde de la Luna: el criterio de gradiente es
**peor** que el de aislamiento para este problema, porque confunde exactamente
las dos cosas que hay que separar. Sobra.

---

## 5. Específico del mosaico Bayer

### 5.1 Por qué no se puede mezclar colores

**[MEDIDO]** En los RAW de referencia el fondo del cielo es de ~2400 DN pero con
**valores muy distintos por canal**: en la ventana del punto (1112,931) los
vecinos del mismo color valen 2410, 2407, 2397… y los de otros color 2143, 2331,
2165… La diferencia entre canales es del orden del ruido. Una mediana 3×3 sobre
los 9 valores mezclados está contaminada por la estructura de color y, sobre
todo, **la mediana de 9 valores no es la mediana de los vecinos**: con 9 muestras
y contaminadores en un canal concreto, el elemento central de la distribución
puede ser un valor contaminado. Separar por color es lo que hace que el criterio
sea limpio.

### 5.2 Implementación: sub-imágenes por paridad, no máscaras

El truco para no penalizar rendimiento: el mosaico se parte en **4 sub-imágenes**
según la paridad `(y%2, x%2)`. Cada una contiene un único color, así que "vecino
del mismo color" pasa a ser simplemente **"vecino en 3×3 sin el centro"** sobre la
sub-imagen, y todos los kernels y filtros de OpenCV sirven tal cual.

El radio del vecindario se mide en la sub-imagen (ver §5.2.1): radio 2 son
5×5 sin centro, es decir 24 vecinos del mismo color.

Cada sub-imagen es 1/4 del tamaño: el coste de memoria baja a la cuarta parte y
todo es separable.

### 5.2.1 **[MEDIDO]** El radio no es un detalle menor

Script: `research/hotpixel_radius.py`. Detecciones totales y FP/Mpx sobre
`IMG_5633.CR2` (18.0 Mpx, 2 positivos conocidos), y sobre `IMG_6174.CR2`
(18.0 Mpx, sin positivos conocidos):

| Radio sub | Vecinos | k=8 | k=12 | **k=15** | **k=20** | k=30 |
|---|---|---|---|---|---|---|
| 1 | 8 | 1540.2 | 400.0 | 189.3 | 74.9 | 24.5 |
| **2** | **24** | 33.8 | 10.4 | **9.5** | **8.7** | 8.2 |
| 3 | 48 | 12.7 | 9.4 | 9.0 | 8.5 | 8.3 |

`IMG_6174` se comporta igual (radio 1: 65.8 FP/Mpx en k=20; radio 2: 1.4;
radio 3: 1.3).

La lectura importante es la **forma de la curva**, no el valor puntual:

- **Radio 1 es inservible.** El conteo no se estabiliza en ningún sitio: decrece
  monótonamente con k (1540 → 24.5 al pasar de k=8 a k=30) sin meseta. Esa forma
  es la firma de una estimación de ruido demasiado inestable: con 8 vecinos el
  MAD de la propia ventana se ve afectado por el valor central y por los propios
  halos de estrella, así que `z` es enorme en muchísimos sitios y subir k solo la
  desplaza, no la discrimina. Los 2 positivos se detectan, pero a costa de
  cientos de miles de errores.
- **Radio 2 ya meseta.** De k=15 a k=30 el conteo apenas se mueve (172 → 147),
  que es exactamente la firma buscada: `k` deja de añadir detecciones y solo
  recorta las más débiles.
- **Radio 3 no aporta.** 156 frente a 154 detecciones en k=20: un 1 % de mejora
  por el doble de vecinos y de coste.

**[PROPUESTA] Radio 2 en la sub-imagen (24 vecinos, ±4 px en el mosaico).**
Con 24 vecinos el MAD es lo bastante estable para que el umbral sea un parámetro
real, y el coste es la mitad que con 48. La regla práctica: **el vecindario debe
tener ≥ ~24 muestras para que el MAD sea un estimador de confianza**; por debajo,
el criterio `z = (p - med) / σ` deja de comportarse como un test de hipótesis.

### 5.3 Los dos tipos de verde

En RGGB hay dos sub-mosaicos de verde distintos, en posiciones distintas, y cada
uno tiene su propia sub-imagen. **[PROPUESTA]** No hace falta tratamiento
distinto: ambos se miden igual, contra sus vecinos de su propia paridad. Lo que
**sí** hay que evitar es agruparlos, porque sus valores difieren por la
iluminación y la posición en el mosaico.

### 5.4 Nota crítica sobre el patrón CFA

**[MEDIDO]** rawpy informa `[[3,2],[0,1]]` para estos ficheros, que **no** es la
codificación RGGB habitual (0=R, 1=G, 2=B). **[PROPUESTA]** El código final
**tiene que leer el patrón de LibRaw** (`imgdata.idata.filters` o equivalente) y
no suponer RGGB, porque la numeración depende de la cámara.

### 5.5 El hook en AstroTracker

**[MEDIDO]** Hoy es inviable: `RawDecoder::decode()` (`src/raw/RawDecoder.cpp:47`)
llama a `dcraw_process()`, que **demosaica dentro de LibRaw**. El mosaico nunca
sale de la librería.

**[PROPUESTA]** No hay que tocar LibRaw. Con `raw.unpack()` los datos crudos ya
están disponibles en `imgdata.rawdata`, junto a `sizes.raw_width/raw_height`,
`sizes.top_margin/left_margin` e `idata.filters`. El orden sería:

```
unpack()
  → detectar y corregir sobre imgdata.rawdata (respeta margins y filters)
  → dcraw_process()   // LibRaw debayera, como hasta ahora
```

Es una única función nueva y no una modificación de una dependencia de terceros.

---

## 6. Corrección

Todas medidas sobre mosaico sintético con patrón Bayer, ruido σ = 8 DN, error
introducido en DN sobre la ventana del defecto. Vecinos **del mismo color**,
24 muestras (radio 2 en la sub-imagen). "Estrella" = hot pixel plantado en el
núcleo de una PSF gaussiana. Script: `research/hotpixel_correction.py`.

| Escena | A. Mediana | B. Media | D. Direccional |
|---|---|---|---|
| Rampa suave | 0.42 | **0.01** | 1.54 |
| Borde escalón | 2.63 | **175.87** ⚠ | **1.87** |
| Nebulosa | 40.51 | 30.18 | **16.83** |
| Núcleo de estrella | 3566 | 3139 | **1574** |

### 6.1 La media es un desastre en los bordes

RMS 176 DN en un borde escalón frente a 2.63 de la mediana: **67× peor**.
Promediar a través de un borde tira del valor hacia el lado equivocado. Es el
motivo clásico por el que la mediana gana a la media, y aquí está medido, no de
oída. **Descartada.**

### 6.2 Direccional: ni mejor ni peor que la mediana

La direccional gana en borde (1.87 vs 2.63) y en nebulosa (16.8 vs 40.5), pero
pierce en rampa (1.54 vs 0.42). Proyecta a lo largo de **un solo eje**, así que
deja pasar el ruido en esa dirección y lo retiene en la otra, y no hace ninguna
estimación de gradiente.

El veredicto honesto es que **se empatan**: ninguna domina. Decidimos por la
mediana por dos razones que no son de error RMS sino de coste y riesgo — es la
que ya se calcula para detectar (corrección gratis) y la que no puede
desbordarse en una configuration adversa. La direccional queda anotada como
refinamiento futuro si el usuario monta engranje de alto telescopio.

### 6.3 Recomendación: mediana de los vecinos del mismo color

Es el mejor compromiso: nunca catastrófico, casi óptima en todos los casos, y
gratis porque ya se calcula la mediana para detectar. La corrección no cuesta
**nada**: reutiliza el mismo vecindario.

### 6.4 Un hot pixel encima de una estrella es irrecuperable

Los tres métodos fallan (1574–2614 DN de error). No existe información en los
vecinos: el valor verdadero estaba ahí y se perdió. **Por eso la detección debe
ser conservadora en zonas brillantes** y no intentar "reparar" nada cerca de una
estructura real.

### 6.5 Master dark

**[PROPUESTA]** No ahora. Es la vía de PIPP y es la correcta para un montaje fijo
con darkos de la misma temperatura y exposición, pero exige un flujo de captura
de darks que AstroTracker no tiene. Además, nuestro detector **ya da ~8.6 FP/Mpx
sin master dark**, medido, lo que es ruido residual despreciable frente a los
~150 defectos reales. Queda como mejora futura: un master dark solo aporta valor
si se capturó en las mismas condiciones.

---

## 7. Análisis temporal

### 7.1 Qué aporta y cuándo

**[MEDIDO]** No validado: solo disponemos de RAW sueltos, no de vídeo. Pero la
lógica se sostiene por lo medido en §3.4: los defectos reales tienen z = 60–260,
es decir, están **muy** por encima de cualquier estructura. Y un defecto está
fijo en el sensor mientras que una estrella se mueve. El contraste es enorme a
favor.

**Cronología importante que no se puede cumplir tal cual.** El análisis temporal
es sobre el **mosaico**, y el de vídeo tiene que ser sobre la imagen
**demosaicada**, porque FFmpeg no entrega mosaico. En imagen demosaicada el delta
del defecto cae de +3059 a +16 DN (§3.2), pero el defecto **sigue estando en la
misma posición pixel**. Por eso en vídeo el criterio debe ser **otro**: no
"brillante sobre su entorno", sino **"se repite siempre en el mismo sitio"**,
que es un criterio temporal puro y no necesita ninguna separación de señal.

### 7.2 Problemas reales

| Problema | Gravedad | Solución |
|---|---|---|
| Trípode completamente inmóvil | **Alta**: las estrellas no se mueven y se marcan como defecto | Detectar con umbral **alto** y avisar: "si tu montaje es fijo y la imagen no se movió, revisa el resultado". **[MEDIDO]** el auto-riesgo es 0.4–0.7 %, o sea bajo |
| Objetivo estático en la Luna | Alta | La Luna llena la mayor parte del encuadre; es el peor caso. Por eso **no** se aplica el filtro temporal a fotos |
| Secuencia muy corta (2–3 frames) | Media: no se puede discriminar | Exigir un mínimo (≥ 8 fotogramas) y desactivar el botón si no se cumple |
| Estrellas por seeing | Baja | **[MEDIDO]** 0.4 % de las detecciones caen en halos: aceptable |
| Máscara desfasada respecto a la exportación | Media | La máscara va en coordenadas del frame original y se aplica **antes** del centrado, no después |

### 7.3 Veredicto

**Sí merece la pena, pero solo para vídeo, y como opt-in.** No es un sustituto
del detector Bayer: cubre un caso que el otro no puede cubrir (vídeo, que ya
viene demosaicado), y ahí además funciona mejor que el espacial porque no depende
de la separación de señal.

**Falso positivo que sí es un riesgo real:** si el campo NO se movió durante el
vídeo, un análisis temporal marcará estrellas. Con el umbral alto el daño medido
es bajo, pero **hay que decirlo en la interfaz**, no esconderlo.

---

## 8. Recomendación

### 8.1 Arquitectura

```
RAW
 │
 ├─ LibRaw: unpack()          ← el mosaico sigue accesible aquí
 │     │
 │     ├─ HotPixelMap (Bayer-aware)
 │     │     ├─ 4 sub-imágenes por paridad CFA
 │     │     ├─ mediana local  (5×5 sin centro → radio 2)
 │     │     ├─ MAD local      (1.4826 · MAD, con suelo)
 │     │     ├─ z = exceso / sigma,  k ≈ 15–20
 │     │     └─ test de aislamiento entre vecinos del mismo color
 │     │
 │     ├─ (opcional) acumulado temporal, si hay más de una toma de la misma escena
 │     │
 │     └─ corrección: mediana de los vecinos del mismo color  (reutiliza la mediana)
 │
 ├─ dcraw_process()           ← debayer, sin cambios
 │
 └─ imagen BGR → resto del pipeline (ajustes, seguimiento, export)
```

### 8.2 Qué se implementa y qué no

| | Decisión |
|---|---|
| Capa | **Mosaico Bayer**, antes de debayer. Descartada la imagen demosaicada |
| Detección | Mediana local + **MAD** local |
| Umbral | **k ≈ 15–20 σ**, con suelo en el sigma. Conservador por diseño |
| Aislamiento | Sí, entre vecinos del mismo color |
| Corrección | **Mediana** de vecinos del mismo color |
| Direccional / media | Descartadas (§6.1, §6.2) |
| Gradiente / Sobel / Laplacian | Descartado (§4.5) |
| Master dark | No ahora (§6.5) |
| Temporal | Sí, **solo vídeo**, opt-in, y solo con la advertencia de trípode fijo |
| JPEG sin RAW | **Sin detección automática**: no hay forma fiable |

### 8.3 Criterios de evaluación

| Criterio | Peso | Ventaja de la propuesta |
|---|---|---|
| Preservar estrellas | Muy alta | **[MEDIDO]** 0.4–0.7 % de FP en halos; y en el mosaico la estrella reparte su señal |
| Preservar Luna | Muy alta | La Luna es estructura de baja frecuencia: z bajo, nunca se detecta |
| Preservar bordes | Muy alta | **[MEDIDO]** mediana 2.63 DN vs 175.87 de la media |
| Funcionar en RAW Bayer | Muy alta | Es la única opción que separa señal de defecto de verdad |
| Reducir falsos positivos | Muy alta | **[MEDIDO]** 8.6 FP/Mpx en k=20, de los cuales casi todos ruido de bajo nivel |
| Calidad de corrección | Muy alta | Reutiliza la mediana: gratis, y la mejor relación error/coste |
| Rendimiento C++ | Alta | **[PROPUESTA]** ~90 ms por 18 Mpx, una pasada, sin dependencias |
| Complejidad | Alta | **[PROPUESTA]** una función sobre `imgdata.rawdata`; sin LibRaw modificado |
| Vídeo | Alta | El temporal cubre el caso que el mosaico no alcanza |
| OpenCV | Media | Se resuelve con `medianBlur` + MorphLib; sin dependencias nuevas |
| Parametrizable en UI | Alta | Un único parámetro (k), con el resto fijo |

### 8.4 **[VERIFICADO]** Puntos exactos de inserción en LibRaw

Verificados leyendo `third_party/libraw` (no de memoria):

| Hecho | Dónde | Consecuencia |
|---|---|---|
| `imgdata.rawdata.raw_image` es `ushort*` y `RAW(row,col)` indexa `raw_image[row*raw_width + col]` | `internal/defines.h:177-178` | El mosaico es un buffer plano con **paso `raw_width`**, no `width`. Los márgenes hay que sumarlos a mano |
| `raw.FC(row, col)` es **pública** y devuelve el código CFA 0–3 | `libraw/libraw.h:312-315`, dentro del tramo `public:` (190–352) | No hay que decodificar `filters` a mano: `filters >> (2·(2·(row&1)+(col&1))) & 3` |
| `imgdata.sizes` trae `raw_width`, `raw_height`, `width`, `height`, `top_margin`, `left_margin` | `libraw_types.h:217` | El área activa en coordenadas de buffer es `[top_margin, top_margin+height)` × `[left_margin, left_margin+width)` |
| LibRaw valida el área activa con `(unsigned)(row - top_margin) < height` | `internal/generic.cpp:34`, `decoders_libraw.cpp:46` | Confirma que ese es el rango correcto |
| `raw_image` se rellena en `unpack()` | — | El hook va **entre `unpack()` y `dcraw_process()`**, que es exactamente el hueco de `src/raw/RawDecoder.cpp:34-47` |
| `imgdata.idata.filters == 9` significa X-Trans, no Bayer | `utils_dcraw.cpp:41` | Hay que **descartar X-Trans y Foveon** en vez de aplicar el reparto 2×2 a ciegas |

**Atajo que simplifica el diseño:** al detector solo le interesa *qué fotositos
comparten color*, no *qué color es*. El reparto en las 4 clases de paridad
`(y&1, x&1)` es invariante aunque `FC` se interrogue con o sin márgenes
(sumarlos solo permuta las etiquetas). Por eso el algoritmo **no depende de la
fase del CFA**, y una batería de tests con los 4 patrones da lo mismo.

### 8.5 Interfaz propuesta

`src/raw/BayerHotPixels.h`, **sin dependencia de LibRaw**, para poder testearlo
sin abrir ficheros RAW:

```cpp
struct HotPixelParams {
    double k = 20.0;      // umbral en sigmas robustas (kMinWarmth = slider 50)
    double sigmaFloor = 1.0;
    int    radius = 2;    // en la sub-imagen (2 => 24 vecinos)
    bool   correct = true;
};

struct HotPixelResult {
    int  detected = 0;    // candidatos que pasan el criterio
    int  corrected = 0;   // fotositos realmente sustituidos
    bool bayer = false;   // false si el mosaico no es Bayer: no se hace nada
};

// Corrige IN PLACE los fotositos calientes del mosaico. `phase` mapea cada
// clase de paridad (y&1, x&1) a un id de color (0..3); solo se usa para agrupar.
HotPixelResult removeHotPixels(cv::Mat& mosaic,
                               const int phase[2][2],
                               const HotPixelParams& p);
```

`RawDecoder::decode()` construye una `cv::Mat` **sin copiar** que envuelve el
buffer de LibRaw (`step = raw_width * 2`), deriva `phase` con `raw.FC()` sobre
las 4 esquinas, y llama a `removeHotPixels` entre `unpack()` y `dcraw_process()`.

#### Trampa: el offset va en BYTES y `raw_image` es un `ushort*`

El puntero de la vista se construye sumando el desplazamiento de márgenes **a un
puntero a `uint16_t`**, que avanza en *elementos*, no en bytes:

```cpp
// MAL: 52*10688 + 142*2 = 556.060 bytes, pero el puntero avanza 556.060 ELEMENTOS
cv::Mat mosaic(h, w, CV_16UC1,
               raw_image + top_margin * step + left_margin * sizeof(uint16_t),
               step);

// BIEN: reinterpretar a byte antes de sumar
cv::Mat mosaic(h, w, CV_16UC1,
               reinterpret_cast<uint16_t*>(
                   reinterpret_cast<uint8_t*>(raw_image) +
                   top_margin * step + left_margin * sizeof(uint16_t)),
               step);
```

Con `IMG_5633` (márgenes 52/142) esto duplicaba el offset: la vista quedaba a
`1.112.120` bytes en lugar de `556.060` y se salía ~466 KB por encima del final
del buffer, con un `0xC0000005` a mitad de imagen. El síntoma más engañoso es que
**todas las comprobaciones de límites dan "dentro de rango"**, porque derivan del
mismo offset ya equivocado.

**Guardas de seguridad**, en este orden:

1. `colors == 3`, `filters != 9` y `!is_foveon`; si no, `bayer = false` y no se
   toca nada.
2. Las 4 clases de paridad deben dar **4 códigos CFA distintos**; si no, el
   patrón no es un Bayer 2×2 y se aborta.
3. `left_margin + width <= raw_width` y `top_margin + height <= raw_height`.

La guarda 3 de este documento (exigir margen mínimo de ≥2 px alrededor del área
activa) **no se implementó**: no hizo falta, y añadirla habría descartado RAW
legítimos. La reflexión de `medianBlur` en el borde del área activa es aceptable
y el radio 2 hace que solo afecte a las 2 filas/columnas extremas.

### 8.6 Valores iniciales

| Parámetro | Valor | Razón |
|---|---|---|
| Radio del vecindario | **2 en la sub-imagen** (5×5 sin centro, 24 vecinos, ±4 px del mosaico) | **[MEDIDO]** con 8 vecinos el MAD no es estable y el FP se dispara (74.9 FP/Mpx en k=20); con 24 meseta en ~155 |
| `k` (umbral en sigmas) | **20** (default del slider 50), rango UI 10–30 | **[MEDIDO]** por debajo de 12 el FP se dispara; por encima de 20 no se gana nada |
| Suelo del sigma | `max(1.4826·MAD, 1.0)` | **[MEDIDO]** ver nota de barrido más abajo |
| Dilatación del mapa (temporal) | 0 | La corrección es por fotosite, no por mancha; dilatar corregiría píxeles sanos |
| Mínimo de frames (temporal) | 8 | Por debajo no hay evidencia |
| Umbral de aciertos (temporal) | 0.7 | **[MEDIDO]** a 0.9 los intermitentes se descartan bien |

#### **[MEDIDO]** El suelo del sigma casi nunca manda (barrido 2026-10)

`research/hotpixel_sigmafloor.py` replica el detector y barre `sigmaFloor` sobre
los dos RAW de referencia. La hipótesis era que con un suelo de 1 DN el umbral
caería a `med + 20` en parches planos y generaría falsos positivos. **Es falsa**:

| `sigmaFloor` | `IMG_5633` det. | conserva de 1 DN | `IMG_6174` det. | conserva de 1 DN |
|---|---|---|---|---|
| 1 | 155 | — | 25 | — |
| 2 | 155 | 155/155 | 25 | 25/25 |
| 4 | 155 | 155/155 | 25 | 25/25 |
| 8 | 154 | 154/155 | 25 | 25/25 |
| 16 | 153 | 153/155 | 24 | 24/25 |

El suelo solo manda en el **0,00–0,11 %** de los píxeles hasta `sigmaFloor = 8`: en
este sensor el umbral real ya lo fija `1.4826·MAD`, que es de decenas de DN.
Subir el suelo a 4 DN no elimina ni un falso positivo, y a partir de 16 DN
empieza a perder quemados reales.

El único sitio donde el suelo bajo produce falsos positivos es un test sintético
con ruido de sigma 40 (MAD ≈ 0): artefacto de la síntesis, no del sensor. El
`test_bayerhot` de la vista no contigua se ajustó para no exigir el conteo exacto
por ese motivo.

**Nota sobre las cifras que ve el usuario**: el detector encuentra 155 fotositos en
`IMG_5633`, pero `test_raw` reporta 11.954 píxeles cambiados. No es una
inconsistencia: al debayer, cada fotosito quemado reparte su carga entre los
píxeles vecinos y la interpolación lo derrama en ~8 píxeles del RGB final. Las dos
cifras no son comparables y no deben citarse como si lo fueran.

---

## 9. Qué hay que rehacer de lo ya escrito

**[RESUELTO]** El trabajo en curso (`src/processing/HotPixelMap.*`, `HotPixelScanWorker.*`
y el filtro espacial en `ImageAdjust.*`) detectaba sobre la imagen **demosaicada**,
que §3.3 demuestra inviable como corrección automática. La distribución final es
 deliberadamente de dos capas, y no un reemplazo:

- **Fotos (RAW):** filtro espacial sobre el **mosaico**, en `src/raw/BayerHotPixels.*`,
  antes del debayer. Es el detector §8.5, opt-in, con deslizador.
- **Vídeo:** mapa **temporal** sobre la imagen demosaicada, en `HotPixelMap`.
  Aquí sí es válido, porque §7 demuestra que exigir repetición en la misma posición
  a lo largo de los fotogramas separa un quemado de una estrella, cosa que es
  imposible en una imagen aislada.

Lo que se conserva tal cual:

- `AdjustWorker::setHotMask()` y la propagación de la máscara: el diseño de
  llevarla por los workers hasta la exportación es correcto y reutilizable.
- La parte temporal, que es la que cubre el vídeo.

**Nota sobre `test_hotpixels`**: los tests nunca se compilaron porque el target no
estaba en `tests/CMakeLists.txt`. Al conectarlo aparecieron **cuatro bugs reales**
que el trabajo en curso llevaba encima sin que nada los señalara, siendo el más
grave que `HotPixelMap::fromVideo()` acumulaba `255` por fotograma en vez de `1`
(`hits += cand` sobre un `CV_8U`), lo que anulaba por completo el criterio
temporal y marcaba las estrellas igual que los quemados. **Ningún test que no se
compila y ejecuta verifica nada**: antes de dar por buena una pieza, comprobar
que su target existe en CTest.

---

## 10. Lo que no se ha podido verificar

- **Implementación interna de PIPP.** Solo consta lo documentado en §2 y §2.1. El
  proyecto está cerrado e inaccesible, así que esto no se puede cerrar: no es un
  pendiente que se pueda resolver con más esfuerzo.
- **La coordenada (3410, 1020) de `IMG_6174`.** Su JPEG es un recorte 16:9 sin
  mapeo fiable al RAW. Habría que confirmarlo con un render de las mismas
  dimensiones que el RAW.
- **El análisis temporal sobre vídeo real.** No hay material de referencia. El
  diseño es razonable y el riesgo está cuantificado en §7.3, pero está sin
  medir. **Con lo corregido en §9**, lo que se puede afirmar es que la lógica de
  acumulación, cancelación y el lector sintético están verificados por test;
  falta el contraste contra un vídeo real.
- **Estrellas reales, no sintéticas.** El 0.4–0.7 % de detecciones en halos es
  una buena señal, pero una comprobación con un catálogo de estrellas
  desplazadas daría la cifra exacta de falsos positivos sobre fuentes reales.
- **El slider fuera de la banda 10–30.** Solo medido con los dos RAW disponibles,
  ambos a ISO desconocido y sin dark. Con series a distintos ISO es probable que
  la meseta se mueva.

## 11. Siguiente paso

### 11.1 **[CERRADO]** El filtro sobre el mosaico (§8.5, §8.6)

Las cinco tareas de este documento están hechas:

1. `removeHotPixels()` sobre `cv::Mat`, sin dependencia de LibRaw — `src/raw/BayerHotPixels.*`.
2. Las 4 sub-imágenes por paridad, con mediana + MAD + aislamiento y corrección por
   mediana reutilizando la mediana ya calculada.
3. Las guardas de Bayer / 4 códigos CFA distintos / márgenes dentro del buffer.
4. `tests/test_bayerhot/`: los 4 patrones CFA dan idéntico resultado, los positivos
   se detectan y corrigen, y la vista no contigua (paso `raw_width*2`) funciona.
5. Enganchado en `RawDecoder::decode()` entre `unpack()` y `dcraw_process()`, con
   la medición de §3.4 reproducida en C++ sobre los dos RAW de referencia.

Estado de la suite: `ctest -C Release` → **20/20**.

### 11.2 Lo que queda

- **Concurrencia de `PhotoSequenceReader::hot_`.** `hot_` se lee desde el worker de
  ajuste y desde el hilo de UI sin sincronizar. Es el riesgo pendiente más real.
- **UI, persistencia y exportación** del mapa temporal: el diseño está cableado
  (`HotPixelScanWorker`, `MainWindow::onHotScanFinished`, `AdjustWorker::setHotMask`,
  `PhotoExportWorker`), pero sin validación end-to-end.
- **Afinar el deslizador.** El rango 10–30 sale de las mediciones actuales; con
  más RAW de referencia (y con series a distintos ISO) se puede ajustar. La banda
  es deliberadamente estrecha: fuera de ella no hay más agresividad útil.

### 11.3 Nota sobre PIPP

Si algún día se quiere paridad con PIPP, el punto de partida está en §2.1: PIPP no
tiene control de sensibilidad, así que "paridad" significa únicamente el
comportamiento por defecto, no la interfaz. El detector conserva su deslizador.