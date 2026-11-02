from pathlib import Path
import html
import json
import textwrap
import subprocess
import sys
from reportlab.pdfgen import canvas
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import Paragraph, Table, TableStyle
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.enums import TA_JUSTIFY,TA_LEFT
from reportlab.lib import colors
from reportlab.lib.pagesizes import A4
from report_content import DATA
BASE=Path(__file__).resolve().parents[1]
F=Path('/System/Library/Fonts/Supplemental')
for name,file in [('Times','Times New Roman.ttf'),('TimesB','Times New Roman Bold.ttf'),('TimesI','Times New Roman Italic.ttf'),('Mono','Courier New.ttf')]:
    pdfmetrics.registerFont(TTFont(name,str(F/file)))
pdfmetrics.registerFontFamily('Times',normal='Times',bold='TimesB',italic='TimesI',boldItalic='TimesB')
W,H=A4;M=57;CW=W-2*M
body=ParagraphStyle('body',fontName='Times',fontSize=12,leading=16,alignment=TA_JUSTIFY,spaceAfter=9)
head=ParagraphStyle('head',fontName='TimesB',fontSize=14,leading=18,spaceAfter=14)
sub=ParagraphStyle('sub',fontName='TimesB',fontSize=12,leading=16,spaceAfter=5)
small=ParagraphStyle('small',fontName='Times',fontSize=10,leading=13,alignment=TA_LEFT)
NAMES={2:'Garland_LR2_V2',3:'Compass_LR3_V2',4:'Compass_RTOS_LR4_V2'}
TITLES={2:'Последовательный интерфейс UART',3:'OLED-дисплей и устройства I2C',4:'Операционные системы реального времени'}
class Report:
    def __init__(self,n):
        self.n=n;self.lab=BASE/f'ЛР{n}';self.project=self.lab/NAMES[n]
        self.path=self.lab/f'ПВС_ЛР{n}_P3431_Мироненко_Амельченко.pdf'
        self.c=canvas.Canvas(str(self.path),pagesize=A4);self.number=0;self.y=H-M;self.md=[]
        self.c.setTitle(f'ПВС — ЛР{n} — Вариант 2');self.c.setAuthor('Мироненко Артем Дмитриевич; Дмитрий Амельченко')
    def footer(self):
        if self.number>1:self.c.setFont('Times',11);self.c.drawCentredString(W/2,30,str(self.number))
    def page(self,title=None):
        if self.number:self.footer();self.c.showPage()
        self.number+=1;self.y=H-M
        if title:self.p(title,head);self.md.append('\n## '+title+'\n')
    def p(self,s,style=body):
        obj=Paragraph(html.escape(s),style);_,height=obj.wrap(CW,1000)
        assert self.y-height>=M,(self.n,self.number,s[:50],self.y,height)
        obj.drawOn(self.c,M,self.y-height);self.y-=height+style.spaceAfter
    def section(self,title,paras):
        self.page(title)
        for label,s in paras:
            self.p(label,sub);self.p(s);self.md.append('### '+label+'\n\n'+s+'\n')
    def table(self,rows,widths):
        cells=[[Paragraph(html.escape(str(v)),small) for v in row] for row in rows]
        table=Table(cells,colWidths=widths,repeatRows=1)
        table.setStyle(TableStyle([('GRID',(0,0),(-1,-1),.5,colors.black),('BACKGROUND',(0,0),(-1,0),colors.HexColor('#eeeeee')),('VALIGN',(0,0),(-1,-1),'TOP'),('LEFTPADDING',(0,0),(-1,-1),6),('RIGHTPADDING',(0,0),(-1,-1),6),('TOPPADDING',(0,0),(-1,-1),5),('BOTTOMPADDING',(0,0),(-1,-1),5)]))
        _,height=table.wrap(CW,1000);assert self.y-height>=M,(self.n,self.number,height)
        table.drawOn(self.c,M,self.y-height);self.y-=height+12
        self.md.extend([' | '.join(map(str,row))+'\n' for row in rows])
    def listing(self,title,path,begin=None,end=None):
        self.page(title);self.p(str(path.relative_to(self.lab)),small)
        source=path.read_text().splitlines()[begin:end]
        self.md.append('```'+('python' if path.suffix=='.py' else 'c')+'\n'+'\n'.join(source)+'\n```\n')
        lines=[]
        for s in source:lines+=textwrap.wrap(s.expandtabs(4),width=108,replace_whitespace=False,drop_whitespace=False,subsequent_indent='    ') or ['']
        size=min(8.0,(self.y-M-20)/max(len(lines),1)/1.17);assert size>=6.7,(self.n,title,len(lines),size)
        self.c.setFont('Mono',size)
        for s in lines:self.y-=size*1.17;self.c.drawString(M,self.y,s)
        self.y-=12
        self.p('Полные исходные коды всех модулей, тестов и клиента входят в папку лабораторной. Здесь приведён фрагмент реализации.',small)
    def cover(self):
        self.page();c=self.c;c.setFont('TimesB',14)
        for i,s in enumerate(['Федеральное государственное автономное образовательное','учреждение высшего образования','Национальный исследовательский университет ИТМО']):c.drawCentredString(W/2,H-66-i*17,s)
        c.drawImage(str(BASE/'tools/assets/itmo-logo.png'),W/2-62,H-185,width=124,height=36,mask='auto')
        c.setFont('Times',12);c.drawCentredString(W/2,H-225,'Факультет Программной Инженерии и Компьютерной Техники')
        c.setFont('TimesB',16);c.drawCentredString(W/2,H-278,f'Лабораторная работа № {self.n}')
        c.setFont('TimesB',14);c.drawCentredString(W/2,H-299,'«'+TITLES[self.n]+'»')
        c.setFont('Times',12);c.drawCentredString(W/2,H-320,'по дисциплине')
        c.setFont('TimesB',14);c.drawCentredString(W/2,H-340,'«Проектирование вычислительных систем»')
        c.drawCentredString(W/2,H-372,'Вариант № 2: «'+('Гирлянда' if self.n==2 else 'Компас')+'»')
        c.setFont('Times',12)
        for i,s in enumerate(['Выполнили: Мироненко Артем Дмитриевич,','Дмитрий Амельченко','Группа: P3431','Преподаватель: Пинкевич Василий Юрьевич']):c.drawRightString(W-M,290-i*18,s)
        c.drawCentredString(W/2,98,'Санкт-Петербург,');c.drawCentredString(W/2,72,'2026')
        self.md.append(f'# Лабораторная работа № {self.n}. {TITLES[self.n]}\n\nВариант 2. Мироненко Артем Дмитриевич; Дмитрий Амельченко. P3431. Преподаватель: Пинкевич Василий Юрьевич. Санкт-Петербург, 2026.\n')
    def save(self):
        self.footer();self.c.save();(self.lab/'report.md').write_text('\n'.join(self.md)+'\n')
        print(self.path.name,self.number,'pages')

for n in (3,4):
    r=Report(n)
    tests=(r.lab/'artifacts/tests.log').read_text()
    assert 'PASS' in tests
    r.cover();r.page('Оглавление')
    extra=['7. Методика и результаты программной проверки','8. Сборка, загрузка и воспроизводимость','9. '+('Состояния и анализ граничных случаев' if n==2 else 'Сравнение реализаций и ограничения'),'10.1. Исходные коды: главный цикл','10.2. Исходные коды: прикладная логика','10.3. Исходные коды: '+('очередь и обработка UART' if n==2 else 'драйвер устройств'),'11. Проверка на стенде, выводы и источники']
    for page,title in enumerate([s[0] for s in DATA[n]]+extra,3):
        r.c.setFont('Times',11.5);r.y-=29;r.c.drawString(M,r.y,title);r.c.drawRightString(W-M,r.y,str(page))
    r.y-=25;r.p('Дата программной проверки: 02.11.2026. Аппаратная проверка вынесена в отдельный протокол. Оформление титульного листа, исполнители и преподаватель сохранены по отчёту первой лабораторной.',body)
    for title,paras in DATA[n]:r.section(title,paras)
    r.page(extra[0]);r.p('Проверки выполнены без подключения к стенду. Тесты запускают реальные C-модули, а аппаратный HAL заменяется имитатором только в тестах драйверов. Компиляция прошивок выполняется настоящим ARM GCC. Прохождение этих проверок не подтверждает работу физических выводов.')
    rows=[['Проверка','Результат и граница доказательства'],['ARM-сборка','HAL для STM32F407 и STM32F427; ELF, HEX, BIN. Для ЛР2 дополнительно REGISTER для обеих моделей.'],['C-логика','Clang -Wall -Wextra -Werror, ASan/UBSan. Ошибок в выполненных сценариях не выявлено.'],['UART/FIFO' if n==2 else 'I²C/OLED','Обёртка HAL UART проверена с имитатором: RX/TX, переход I/P после завершения TX, счётчики переполнений.' if n==2 else 'Имитатор проверяет адреса, WHO_AM_I DS1/DS0, reset с сохранением P1…P7, 1024 байта кадра, ошибки и повтор.'],['Команды' if n==2 else 'Клиент ПК','CR/LF/CRLF, длины, ошибки, периоды, выбор слота, циклическая перезапись 5…8, сохранение времени.' if n==2 else '3 Python-теста: протокол, demo и логи, pyserial через PTY с разбиением строк; все прошли.'],['Структура ELF','Проверены таблица векторов, адрес стека, heap; в ЛР4 — SVC/PendSV/SysTick и символы FreeRTOS.'],['OpenOCD','Разбор конфигураций .407/.427 с -c shutdown: успешен; к адаптеру не подключался.']]
    r.table(rows,[105,CW-105]);r.p('Фактические логи: artifacts/tests.log, build_*.log, openocd_*.log. SHA256 всех готовых образов: artifacts/sha256.json. Повторить комплект: ../verify.sh. Испытания на плате: ПРОВЕРКА_НА_ПЛАТЕ.md.',small)
    r.section(extra[1],[
      ('Инструменты','Arm GNU Toolchain 14.2.Rel1 (AArch64 macOS), OpenOCD 0.12.0, Clang с AddressSanitizer и UndefinedBehaviorSanitizer; клиент Python 3.10 с pyserial 3.5. Версии исходников HAL, CMSIS и FreeRTOS закреплены commit SHA в Drivers/versions.json.'),
      ('Сборка','Импортировать '+NAMES[n]+' в STM32CubeIDE как Existing Project либо выполнить make -C '+NAMES[n]+'. Для другой модели задать MCU=STM32F407xx. Внешний Makefile автоматически ищет компилятор IDE, затем локальный tools/toolchain. STM32CubeMX и загрузка пакета HAL не требуются.'),
      ('Прошивка','Из папки лабораторной выполнить ./flash.sh; для .407 — MCU=STM32F407xx ./flash.sh. Запускается сборка, OpenOCD через FTDI/JTAG, program verify reset exit. Конфигурации различают модели, в .427 добавлен boundary-scan TAP. Перед реальной записью сверить маркировку.'),
      ('Отладка','./flash.sh --debug запускает GDB-сервер. Подключиться к localhost:3333, reset halt, load, break main, continue. Проверить достижение main, работу TIM6 и обработчиков. При остановке отладчиком исполнение приложения прекращается, поэтому FPS измеряется без остановок.'),
      ('Запуск клиента','Терминал на 38400 8N1, local echo off; выполнить new gryn, период medium, затем set 5. Для проверки части 2 пересобрать и прошить UART_BACKEND=REGISTER.' if n==2 else '../tools/venv/bin/python client/client.py PORT --log uart.jsonl. Без платы — --demo с отдельным журналом. В журнале demo=true отличает синтетические данные. Установка на другом ПК описана в README.md.')])
    if n==2:
        r.section(extra[2],[('Состояния парсера','Обычный ввод накапливается до Enter. После корректного new активируется состояние ожидания периода; неверный период оставляет это состояние для повторного ввода. После сохранения возвращается разбор команд. Поэтому команды set следует вводить после завершения ввода периода.'),('CRLF и переполнение','CR обрабатывает строку, следующий LF подавляется. Слишком длинная строка помечается overflow, не выполняется и сбрасывается по Enter. FIFO не допускает записи за пределы массива. Диагностические счётчики позволяют заметить потерю при чрезмерном потоке.'),('Конкурентность','RX — ISR-производитель и foreground-потребитель; TX — foreground-производитель и ISR-потребитель. Счётчики volatile, публикация индекса отделена компиляторным барьером. Начало HAL-передачи защищено короткой критической секцией; передача продолжается цепочкой callbacks.'),('Переход I/P','До отключения ISR ожидаются пустой TX, отсутствие HAL tx_busy и аппаратный TC. Затем прекращается приём HAL IT, меняются источники и NVIC. Очереди не обнуляются, уже принятые данные сохраняются. Переход проверен имитатором, физические фронты ещё не измерены.'),('Ограничение','Конечная очередь не может гарантировать приём бесконечного потока при отсутствии flow control. В отчёте и README описаны размер, диагностика и ручной сценарий проверки; неконтролируемая высокоскоростная нагрузка не выдаётся за безусловно поддерживаемую.')])
    else:
        r.section(extra[2],[('Без ОС','ЛР3: один управляющий цикл, последовательное владение I²C, предсказуемый порядок обработки и меньше RAM. Для отзывчивости нужно вручную разбивать передачу экрана и регулярно вызывать UART service. Рост числа функций усложняет расписание.'),('С ОС','ЛР4: две задачи разделяют обмен с ПК и работу с устройствами, а вытеснение обеспечивает обслуживание UART независимо от текущего блока OLED. Очереди дают целостные снимки. Требуются отдельные стеки, heap, правильные ISR-приоритеты и анализ загрузки.'),('Частота кадров','Целевой период обеих версий — 40 мс. В ЛР4 vTaskDelayUntil не добавляет длительность работы к каждому периоду. Если работа превысит бюджет, планировщик не может гарантировать FPS сам по себе. Счётчик frames и frame_ms позволяют проверить фактическое поведение; итоговые замеры пока отсутствуют.'),('Выбор синхронизации','Один владелец I²C исключает перекрытие транзакций без мьютекса. Очереди длины 1 подходят для последнего измерения и последнего набора коэффициентов; старые значения не нужно воспроизводить. Поток UART сохраняет порядок в обычном FIFO.'),('Границы','Не реализованы бонусная клавиатура, коррекция наклона и полная магнитная калибровка. Включены распознавание DS1/DS0 и выявление устаревших данных. Положение осей, географическое направление и минимальные FPS нужно подтвердить на стенде.')])
    r.listing(extra[3],r.project/'Core/Src/main.c')
    r.listing(extra[4],r.project/('Core/Src/garland.c' if n==2 else 'Core/Src/compass.c'))
    if n==2:r.listing(extra[5],r.project/'Core/Src/serial.c',0,55)
    else:r.listing(extra[5],r.project/'Core/Src/devices.c',20,72)
    r.page(extra[6])
    r.p('План испытаний',sub)
    for s in (['Сверить MCU, прошить и подтвердить Flash verify, проверить запуск после RESET.','Проверить четыре режима, кнопку, удержание и восстановление кадров.','Добавить четыре записи, проверить перезапись 5→8 и ошибочные команды.','В обеих прошивках HAL/REGISTER проверить эхо, I/P и счётчики ошибок.'] if n==2 else ['Подтвердить PONG, sensor_ok=1 и oled_ok=1, сравнить компоненты OLED/ПК.','Проверить масштабы 0/100/200%, все нули и поворот платы на столе.','Сопоставить реальные оси с направлением стрелки, проверить локальный магнит.','Наблюдать F:99→00 и измерить delta_frames/delta_tick за ≥10 с, приложить UART-лог.']):r.p('• '+s,small)
    r.y-=8;r.p('Выводы',sub)
    r.p(('Подготовлены две реализации UART и приложение гирлянды с восемью слотами. Сборка и программные тесты подтверждают предусмотренные алгоритмы и границы данных. ' if n==2 else 'Подготовлены компас, драйверы I²C/OLED/магнитометра и клиент с журналом. '+('Выполнен перенос на две задачи FreeRTOS с очередями снимков и независимым HAL tick. ' if n==4 else 'Обмен устройствами организован в одном главном цикле без конфликтов I²C. '))+'Работа физических интерфейсов и аппаратные характеристики пока не подтверждены. Протокол наблюдений вынесен в ПРОВЕРКА_НА_ПЛАТЕ.md.',body)
    r.p('Источники',sub)
    sources=['Учебное пособие SDK-1.1M_26101101.pdf, разделы 1.3–1.8, 5.2–5.4.','STMicroelectronics RM0090: www.st.com/resource/en/reference_manual/rm0090-stm32f407-advanced-armbased-32bit-mcus-stmicroelectronics.pdf','HAL STM32F4: github.com/STMicroelectronics/stm32f4xx-hal-driver']
    if n>2:sources+=['LSM9DS1/LSM9DS0: www.st.com/resource/en/datasheet/lsm9ds1.pdf; lsm9ds0.pdf.','Аппаратная платформа: github.com/lmtspbru/SDK-1.1M.']
    if n==4:sources+=['FreeRTOS Kernel V11.2.0: github.com/FreeRTOS/FreeRTOS-Kernel/tree/V11.2.0.']
    for s in sources:r.p(s,small)
    r.md.append('\n### Испытания и выводы\n\n'+(r.lab/'ПРОВЕРКА_НА_ПЛАТЕ.md').read_text()+'\n### Источники\n\n'+'\n'.join(sources))
    assert r.number==15
    r.save()
