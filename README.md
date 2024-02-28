<p align="right"><a href="https://certification.oshwa.org/by000001.html"><img src="assets/certification-mark-BY000001-wide.svg"/></a></p>

# Спутник Плутона -- ХАРОН

Предварительные каскады для Analog Devices ADALM-PLUTO SDR.

https://wiki.analog.com/university/tools/pluto

* Передняя панель управления и индикации
* Задняя панель с разъёмами
* Основная плата

## Панели v1.0.0

### Вид спереди / сзади


<img src="hw/front.svg"></img>
<img src="hw/back.svg"></img>

Исходные файлы для печатной платы:
* hw/front.svg
* hw/back.svg

Для получения KiCAD проекта используйте Inkscape + svg2shenzhen

https://github.com/badgeek/svg2shenzhen

Для получения GERBER файлов используйте KiCAD

Для надписей использован шрифт B612:

https://fonts.google.com/specimen/B612

### Перечень элементов

| Поз.<br> обозна- <br>чение | Наименование | Кол. | Примечание |
|-|-|-|-|
| D1 | VS1838B | 1 | |
| R1 | Переменный резистор 10К | 1 | |
| ИН1 | Индикатор 2" TFT ST7789V | 1 | |
| SB1 SB2 | Кнопка 6x6x5 | 2 | |
| Р1 - Р9 | SMA -> IPEX | 9 | |
| Р10 | DB9 | 1 | |

### Передняя панель

<img src="assets/front1.jpg"></img>

### Вид спереди в корпусе

<img src="assets/front2.jpg"></img>

### Задняя панель

<img src="assets/back1.jpeg"></img>

## Основная плата v1.0.0

### Схема антенной секции

<img src="hw/ANT.png"></img>

### Схема процессорной секции

<img src="hw/CPU.png"></img>

### Схема секции источник питания

<img src="hw/FEM.png"></img>

### Схема секции фильтров

<img src="hw/FILTERS.png"></img>

### Схема секции усилителя

<img src="hw/PA.png"></img>

### Схема секции приёмника

<img src="hw/RX.png"></img>

### Топология платы лицевая сторона

<img src="hw/8B_TOP.png"></img>

### Топология платы обратная сторонв

<img src="hw/8B_BOT.png"></img>

## Корпус

<img src="assets/case4.webp"></img>

## Индикатор

<img src="assets/tft-mechanical.webp"></img>

## Фото устройства

<img src="assets/XAPOH N4h.jpg"></img>

<img src="assets/XAPOH25.jpeg"></img>

<img src="assets/XAPOH N4e.jpg"></img>

## Преречень элементов

hw/8B_PE.doc

## License

Hardware is released under the [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/) license.

Software is relesed under [MIT](LICENSE) license.

