"""Create offline-readable protocol diagrams and an HTML reading edition.

Regeneration only needs Python, Pillow and Markdown; opening the delivered HTML
or PNG diagrams needs no Python, browser extension, network or JavaScript.
"""
from pathlib import Path
import html
import math
import re
import xml.etree.ElementTree as ET

import markdown
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
DOC = ROOT / "docs" / "protocol-architecture-guide.md"
ASSETS = ROOT / "docs" / "assets" / "protocols"
PALETTE = {
    "app": ("#eff6ff", "#2563eb"),
    "driver": ("#f0fdfa", "#0f766e"),
    "hardware": ("#fff7ed", "#b45309"),
    "neutral": ("#f8fafc", "#64748b"),
}
FONT_DIR = Path("C:/Windows/Fonts")
REGULAR = FONT_DIR / "segoeui.ttf"
BOLD = FONT_DIR / "segoeuib.ttf"


class Figure:
    def __init__(self, name, title, height, width=940):
        self.name, self.width, self.height = name, width, height
        self.image = Image.new("RGB", (width * 2, height * 2), "white")
        self.draw = ImageDraw.Draw(self.image)
        self.svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" '
                    f'height="{height}" viewBox="0 0 {width} {height}" role="img" '
                    f'aria-labelledby="{name}-title">',
                    f'<title id="{name}-title">{html.escape(title)}</title>',
                    '<rect width="100%" height="100%" fill="white"/>']
        self.text(width / 2, 28, title, 24, bold=True)

    def text(self, x, y, value, size=17, color="#172b43", bold=False, align="middle"):
        font = ImageFont.truetype(str(BOLD if bold else REGULAR), size * 2)
        anchor = "mm" if align == "middle" else "lm"
        self.draw.text((x * 2, y * 2), value, font=font, fill=color, anchor=anchor)
        self.svg.append(f'<text x="{x}" y="{y}" font-family="Segoe UI,Arial,sans-serif" '
                        f'font-size="{size}" font-weight="{600 if bold else 400}" '
                        f'text-anchor="{align}" dominant-baseline="middle" '
                        f'fill="{color}">{html.escape(value)}</text>')

    def node(self, x, y, width, title, lines=(), kind="driver", height=96):
        fill, stroke = PALETTE[kind]
        self.draw.rounded_rectangle((x * 2, y * 2, (x + width) * 2, (y + height) * 2),
                                    radius=18, fill=fill, outline=stroke, width=3)
        self.svg.append(f'<rect x="{x}" y="{y}" width="{width}" height="{height}" '
                        f'rx="9" fill="{fill}" stroke="{stroke}" stroke-width="1.5"/>')
        self.text(x + width / 2, y + 24, title, 19, bold=True, color=stroke)
        for i, line in enumerate(lines):
            font = ImageFont.truetype(str(REGULAR), 34)
            if self.draw.textlength(line, font=font) > (width - 22) * 2:
                raise ValueError(f"Label too wide in {self.name}: {line}")
            self.text(x + width / 2, y + 52 + i * 23, line)
        return (x, y, width, height)

    def arrow(self, points, label=None, label_at=None, dashed=False, color="#64748b"):
        scaled = [(x * 2, y * 2) for x, y in points]
        self.draw.line(scaled, fill=color, width=3)
        self.svg.append(f'<polyline points="{" ".join(f"{x},{y}" for x, y in points)}" '
                        f'fill="none" stroke="{color}" stroke-width="1.5" '
                        + ('stroke-dasharray="6 5" ' if dashed else '') + '/>')
        (x0, y0), (x1, y1) = points[-2:]
        angle = math.atan2(y1 - y0, x1 - x0)
        ends = [(x1, y1),
                (x1 - 9 * math.cos(angle - .45), y1 - 9 * math.sin(angle - .45)),
                (x1 - 9 * math.cos(angle + .45), y1 - 9 * math.sin(angle + .45))]
        self.draw.polygon([(x * 2, y * 2) for x, y in ends], fill=color)
        self.svg.append(f'<polygon points="{" ".join(f"{x},{y}" for x, y in ends)}" fill="{color}"/>')
        if label:
            self.text(*label_at, label, 15, color=color)

    def save(self):
        self.svg.append("</svg>")
        svg = "\n".join(self.svg)
        ET.fromstring(svg)
        (ASSETS / f"{self.name}.svg").write_text(svg, encoding="utf-8")
        self.image.save(ASSETS / f"{self.name}.png", optimize=True)
        return svg


def layers():
    f = Figure("layers", "Từ yêu cầu ứng dụng tới tín hiệu trên dây", 945)
    items = [
        ("1. APPLICATION", ["Hiểu lệnh và ý nghĩa dữ liệu", "Ví dụ: LED ON, đọc giá trị cảm biến"], "app"),
        ("2. SERVICE / PROTOCOL", ["Ghép byte thành lệnh hoặc gói", "Parser, request và response"], "app"),
        ("3. DRIVER", ["Quản lý handle, buffer và trạng thái", "UART / SPI / I2C / CAN"], "driver"),
        ("4. THANH GHI", ["Driver ghi cấu hình và data register", "CR, ISR/SR, TDR/RDR, DMA"], "hardware"),
        ("5. NGOẠI VI TRONG MCU", ["Phần cứng tự tạo clock, dịch bit, nhận byte", "USART2 / SPI1 / I2C1 / bxCAN"], "hardware"),
        ("6. CHÂN VÀ MẠCH NGOÀI", ["GPIO alternate function nối ngoại vi ra chân", "Dây, thiết bị, pull-up hoặc transceiver"], "hardware"),
    ]
    for i, (title, body, kind) in enumerate(items):
        y = 82 + i * 132
        f.node(38, y, 554, title, body, kind)
        if i:
            f.arrow([(315, y - 36), (315, y)])
    f.node(640, 82, 260, "BOARD CONFIG", ["Chọn chân, AF, clock", "GPIO_Init trước Init"], "neutral")
    f.arrow([(770, 178), (770, 366), (592, 366)], "Cấu hình", (836, 254))
    f.node(640, 500, 260, "IRQ / NVIC", ["Báo có byte hoặc lỗi", "ISR cập nhật driver"], "neutral")
    f.arrow([(592, 658), (770, 658), (770, 596)], "Sự kiện", (751, 682), True)
    f.arrow([(770, 500), (770, 420), (592, 420)], "Byte / trạng thái", (856, 464), True)
    f.text(470, 910, "Đọc từ trên xuống. Kết quả được driver trả lên qua Read, callback hoặc TX result.", 17)
    return f


def startup():
    f = Figure("startup", "Cùng phần cứng, hai cách tổ chức chương trình", 690)
    f.node(260, 70, 420, "RESET → MAIN", ["Khởi tạo RAM và timebase 1 ms"], "hardware")
    f.arrow([(470, 166), (470, 204), (232, 204), (232, 240)])
    f.arrow([(470, 204), (710, 204), (710, 240)])
    f.node(32, 240, 400, "BARE-METAL", ["CommunicationDemo_Init", "Một vòng lặp main gọi các Process"], "app")
    f.node(510, 240, 400, "FREERTOS", ["Tạo task, queue và mutex tĩnh", "vTaskStartScheduler"], "app")
    f.arrow([(232, 336), (232, 390)])
    f.node(32, 390, 400, "MAIN SỞ HỮU CÁC DRIVER", ["Luân phiên UART / SPI / I2C / CAN", "Callback chạy trong main"], "driver")
    f.arrow([(710, 336), (710, 390)])
    f.node(510, 390, 400, "TASK CÓ QUYỀN SỞ HỮU", ["Comm: UART / SPI / CAN", "I2CWorker: I2C; Producer: gửi request"], "driver")
    f.text(470, 553, "Driver và thanh ghi không đổi theo cách lập lịch.", 20, bold=True)
    f.text(470, 592, "Bare-metal nhường bằng cách return. Task nhường bằng delay hoặc chờ sự kiện.", 17)
    return f


def uart():
    f = Figure("uart-rx", "UART RX: hai cách nhận, một parser ứng dụng", 850)
    f.node(260, 70, 420, "PA3 → USART2 RDR", ["Một byte đến từ đường UART"], "hardware")
    f.arrow([(470, 166), (470, 196), (235, 196), (235, 230)])
    f.arrow([(470, 196), (705, 196), (705, 230)])
    f.node(35, 230, 400, "CHỌN RX IRQ", ["RXNE → UART_IRQHandling", "ISR đọc byte từ RDR"], "driver")
    f.node(505, 230, 400, "HOẶC RX DMA", ["DMA ghi vào buffer raw 256 byte", "HT / TC / IDLE báo phần dữ liệu mới"], "driver")
    f.arrow([(705, 326), (705, 370)])
    f.node(505, 370, 400, "COLLECTOR TRONG ISR", ["Copy phần mới từ raw buffer", "Không đọc trùng phần đã lấy"], "driver")
    f.arrow([(235, 326), (235, 502), (470, 502), (470, 530)])
    f.arrow([(705, 466), (705, 502), (470, 502)])
    f.node(220, 530, 500, "RX RING → UART_Read", ["Main hoặc Comm task lấy byte từ ring", "Chưa có ý nghĩa lệnh ở bước này"], "driver")
    f.arrow([(470, 626), (470, 674)])
    f.node(220, 674, 500, "PARSER → APPLICATION", ["Ghép đến LF: LED ON, PING, ...", "Hiểu lệnh rồi thực hiện công việc"], "app")
    f.text(470, 816, "RXNE và DMA là hai lựa chọn thay thế; không cùng tranh đọc RDR.", 17)
    return f


def spi():
    f = Figure("spi", "SPI: nhận yêu cầu trước, báo kết quả sau", 910)
    items = [
        ("1. APPLICATION SUBMIT", ["SPI_Submit copy descriptor vào queue", "TX/RX buffer phải sống tới callback"], "app"),
        ("2. PROCESS BẮT ĐẦU", ["Lấy transaction, đặt mode và tốc độ", "Enable SPI, kéo CS xuống thấp"], "driver"),
        ("3. NGOẠI VI TRAO ĐỔI BIT", ["SCK tạo nhịp, MOSI gửi, MISO nhận", "Một byte TX đi cùng một byte RX"], "hardware"),
        ("4. IRQ CẬP NHẬT TIẾN ĐỘ", ["TXE ghi DR; RXNE đọc DR", "Lặp từng byte đến đủ length"], "driver"),
        ("5. PROCESS ĐỢI HẾT BẬN", ["Đủ RX, TX FIFO rỗng, BSY = 0", "Nhả CS; timeout thì báo lỗi"], "driver"),
        ("6. CALLBACK VỀ APPLICATION", ["spi_complete xem dữ liệu hoặc lỗi", "Chạy trong main / Comm task, ngoài ISR"], "app"),
    ]
    for i, (title, body, kind) in enumerate(items):
        y = 80 + i * 132
        f.node(170, y, 600, title, body, kind)
        if i:
            f.arrow([(470, y - 36), (470, y)])
    return f


def i2c():
    f = Figure("i2c", "I2C: ghi register rồi repeated START để đọc", 850)
    f.node(250, 70, 440, "IDLE → LẤY TRANSACTION", ["Descriptor chứa address, TX/RX, timeout"], "app")
    f.arrow([(470, 166), (470, 210)])
    f.node(250, 210, 440, "WAIT_BUS", ["Đợi bus rảnh, có thời hạn", "Không làm kẹt main hoặc task"], "driver")
    f.arrow([(470, 306), (470, 337), (230, 337), (230, 375)])
    f.node(30, 375, 400, "TX: WRITE REGISTER", ["START + address WRITE", "TXIS: gửi byte địa chỉ register"], "hardware")
    f.node(510, 375, 400, "RX: READ DATA", ["START + address READ", "RXNE: lấy byte dữ liệu"], "hardware")
    f.arrow([(430, 423), (510, 423)])
    f.text(710, 346, "TC → repeated START, không STOP", 17, bold=True)
    f.arrow([(710, 471), (710, 518), (470, 518), (470, 555)])
    f.node(250, 555, 440, "STOPF → DONE", ["Phần cứng đã kết thúc transaction", "Process mới gọi callback về ứng dụng"], "driver")
    f.text(470, 714, "TX-only và RX-only cũng được hỗ trợ.", 19, bold=True)
    f.text(470, 756, "NACK / lỗi / timeout đi theo đường kết thúc hữu hạn; không chờ vô hạn.", 17)
    f.text(470, 793, "ACK/NACK byte cuối khác với ý nghĩa thành công của lệnh ứng dụng.", 17)
    return f


def can():
    f = Figure("can", "CAN: queue nguyên frame, hai đường RX và TX result", 810)
    f.node(30, 80, 280, "APPLICATION GỬI", ["CAN_Send copy frame"], "app")
    f.node(630, 80, 280, "APPLICATION NHẬN", ["CAN_Receive đọc frame"], "app")
    f.node(30, 225, 280, "TX QUEUE", ["Chờ mailbox trống"], "driver")
    f.node(630, 225, 280, "RX QUEUE", ["ISR copy rồi release FIFO"], "driver")
    f.node(30, 370, 280, "TX MAILBOX", ["Giao frame cho hardware"], "hardware")
    f.node(630, 370, 280, "FILTER + RX FIFO", ["Nhận frame được chọn"], "hardware")
    f.node(330, 515, 280, "bxCAN CONTROLLER", ["Truyền và nhận frame"], "hardware")
    f.node(330, 665, 280, "MẠCH NGOÀI", ["Transceiver → CAN bus", "Demo dùng loopback nội bộ"], "hardware")
    f.node(30, 665, 280, "TX RESULT", ["CAN_GetTxResult", "Biết OK hoặc lỗi của frame"], "driver")
    for y in (176, 321):
        f.arrow([(170, y), (170, y + 49)])
        f.arrow([(770, y + 49), (770, y)])
    f.arrow([(170, 466), (170, 563), (330, 563)])
    f.arrow([(610, 563), (770, 563), (770, 466)])
    f.arrow([(470, 611), (470, 665)])
    f.arrow([(330, 589), (170, 589), (170, 665)], "RQCP / lỗi", (174, 622), True)
    return f


def rtos():
    f = Figure("i2c-rtos", "Đọc I2C trong RTOS: ai làm gì, theo thứ tự nào?", 1120)
    items = [
        ("1. COMM TASK", ["Nhận lệnh I2C READ", "Queue copy address + register"], "app"),
        ("2. I2C WORKER", ["Nhận request; giữ TX/RX buffer", "I2C_Submit rồi I2C_Process"], "app"),
        ("3. DRIVER → HARDWARE", ["Đặt CR2, gửi START và byte register", "Ngoại vi tạo tín hiệu trên SCL / SDA"], "hardware"),
        ("4. I2C EV / ER ISR", ["Driver chuyển byte và cập nhật trạng thái", "Nhả critical section rồi notify Worker"], "driver"),
        ("5. I2C WORKER THỨC DẬY", ["I2C_Process kiểm tra state hoặc timeout", "Callback báo kết quả trong Worker"], "driver"),
        ("6. RESPONSE QUEUE", ["Copy status và byte nhận được", "Worker không tự in UART"], "app"),
        ("7. COMM TASK", ["Nhận response, hiểu kết quả", "Gửi thông báo qua driver UART"], "app"),
    ]
    for i, (title, body, kind) in enumerate(items):
        y = 80 + i * 142
        f.node(160, y, 620, title, body, kind)
        if i:
            f.arrow([(470, y - 46), (470, y)])
    f.text(470, 1062, "Notification là lời nhắc kiểm tra trạng thái, không phải packet dữ liệu.", 17)
    return f


def gpio_events():
    f = Figure("gpio-events", "GPIO input: từ nút nhấn tới application", 770)
    items = [
        ("1. NÚT NGOÀI → PB0", ["Nút nối PB0 xuống GND; pull-up nội giữ mức 1 khi nhả", "Nhấn = 0, nhả = 1; tiếp điểm có thể bị dội"], "hardware"),
        ("2. GPIO + SYSCFG + EXTI", ["MODER input, PUPDR pull-up, EXTICR chọn port B", "RTSR / FTSR bắt hai cạnh; IMR cho phép báo ngắt"], "hardware"),
        ("3. EXTI0 IRQ → DRIVER", ["IO_Demo_GPIOIRQ → GPIO_IRQHandling(0)", "Xóa PR bằng W1C; chỉ đếm cạnh, không delay trong ISR"], "driver"),
        ("4. MAIN / COMM TASK", ["IO_Demo_Process chờ 30 ms từ cạnh được quan sát cuối", "Xác nhận mức ổn định; đếm lần nhấn hoặc in UART"], "app"),
    ]
    for i, (title, body, kind) in enumerate(items):
        y = 85 + i * 156
        f.node(100, y, 740, title, body, kind, height=106)
        if i:
            f.arrow([(470, y - 50), (470, y)])
    f.text(470, 740, "GPIO READ xem mức tức thời; GPIO WATCH ON xem sự kiện đã debounce.", 17)
    return f


def timer_pwm():
    f = Figure("timer-pwm", "Timer: bộ đếm cho periodic IRQ và PWM", 790)
    f.node(185, 80, 570, "BOARD: CLOCK TIMER = 8 MHz", ["PSC chia clock; CNT đếm từ 0 tới ARR", "Update event nạp preload rồi bắt đầu chu kỳ mới"], "hardware", height=106)
    f.arrow([(470, 186), (470, 220), (235, 220), (235, 260)])
    f.arrow([(470, 220), (705, 220), (705, 260)])
    f.node(35, 260, 400, "TIM3: PERIODIC IRQ", ["PSC = 7999, ARR = 99", "10 Hz → mỗi 100 ms có update"], "hardware", height=106)
    f.node(505, 260, 400, "TIM2 CH1: PWM", ["PSC = 7, ARR = 999", "1 kHz; CCR1 chọn thời gian high"], "hardware", height=106)
    f.arrow([(235, 366), (235, 415)])
    f.arrow([(705, 366), (705, 415)])
    f.node(35, 415, 400, "UIF → TIM3_IRQHandler", ["TIM_IRQHandling đếm update", "TIM_Process gọi callback ở task"], "driver", height=106)
    f.node(505, 415, 400, "CNT < CCR1 → HIGH", ["PWM mode 1, active high", "AF1 đưa TIM2_CH1 ra PA0"], "hardware", height=106)
    f.arrow([(235, 521), (235, 570)])
    f.arrow([(705, 521), (705, 570)])
    f.node(35, 570, 400, "APPLICATION", ["TIMER STATS xem số update", "ISR không in UART hay xử lý lệnh"], "app", height=106)
    f.node(505, 570, 400, "APPLICATION", ["PWM 25 → CCR1 = 250", "PWM START phát xung, không cần IRQ"], "app", height=106)
    f.text(470, 740, "SysTick vẫn tạo tick hệ thống; TIM3 và TIM2 là hai bộ timer độc lập.", 17)
    return f


def adc_pwm():
    f = Figure("adc-pwm", "ADC: điện áp → số → quyết định PWM", 945)
    items = [
        ("1. PHẦN CỨNG NGOÀI", ["Biến trở: hai đầu 3V3 / GND, con chạy → PA1", "GPIO analog nối PA1 tới ADC1_IN2"], "hardware"),
        ("2. ADC_INIT", ["ADC clock HCLK/2 = 4 MHz; bật regulator, calibration", "Chọn sample time, kênh và regular sequence dài 1"], "driver"),
        ("3. ADC READ → ADC_Start_IT", ["ADSTART bắt đầu lấy mẫu rồi chuyển đổi 12-bit", "Kết quả 0..4095 nằm trong DR; EOC báo có mẫu"], "hardware"),
        ("4. ADC1_2_IRQHandler", ["ADC_IRQHandling đọc DR, cập nhật Value và Done", "Không tính mV, không in UART, không gọi callback"], "driver"),
        ("5. MAIN / COMM TASK", ["ADC_Process gọi adc_complete; application tính mV", "ADC PWM ON: đổi mẫu thành duty rồi cập nhật TIM2"], "app"),
    ]
    for i, (title, body, kind) in enumerate(items):
        y = 80 + i * 155
        f.node(100, y, 740, title, body, kind, height=106)
        if i:
            f.arrow([(470, y - 49), (470, y)])
    f.text(470, 910, "Mỗi ADC READ lấy một mẫu. Đây chưa phải vòng điều khiển chạy liên tục.", 17)
    return f


FIGURES = [
    (layers, "Sơ đồ các lớp từ application tới phần cứng"),
    (startup, "Khởi động bare-metal và FreeRTOS"),
    (uart, "Luồng nhận UART bằng IRQ hoặc DMA"),
    (spi, "Luồng SPI từ Submit đến callback"),
    (i2c, "Luồng đọc I2C bằng repeated START"),
    (can, "Luồng frame CAN RX và TX result"),
    (rtos, "Luồng I2C giữa Comm task, Worker, driver và ISR"),
    (gpio_events, "GPIO input và debounce trong application"),
    (timer_pwm, "Timer periodic IRQ và PWM phần cứng"),
    (adc_pwm, "Luồng ADC từ chân analog tới callback và PWM"),
]


def main():
    ASSETS.mkdir(parents=True, exist_ok=True)
    rendered = {}
    for create, caption in FIGURES:
        figure = create()
        rendered[figure.name] = (figure.save(), caption)

    source = DOC.read_text(encoding="utf-8")
    blocks = list(re.finditer(r"```mermaid\s*\n.*?\n```", source, re.S))
    if blocks:
        if len(blocks) != len(FIGURES):
            raise ValueError("Update FIGURES to match the document diagrams")
        for block, (create, caption) in reversed(list(zip(blocks, FIGURES))):
            name = create.__name__
            name = {"uart": "uart-rx", "rtos": "i2c-rtos"}.get(name, name)
            replacement = (f"![{caption}](assets/protocols/{name}.png)\n\n"
                           f"[Mở sơ đồ lớn](assets/protocols/{name}.svg)")
            source = source[:block.start()] + replacement + source[block.end():]
        DOC.write_text(source, encoding="utf-8", newline="\n")

    body = markdown.markdown(source, extensions=["tables", "fenced_code", "sane_lists"])
    for name, (svg, caption) in rendered.items():
        image_pattern = rf'<p><img\s+[^>]*src="assets/protocols/{name}\.png"[^>]*\s*/?></p>'
        figure_html = (f'<figure><div class="diagram">{svg}</div>'
                       f'<figcaption>{html.escape(caption)}</figcaption></figure>')
        body, count = re.subn(image_pattern, lambda _: figure_html, body)
        if count != 1:
            raise ValueError(f"Expected exactly one diagram reference for {name}, got {count}")
    body = re.sub(r'<p><a href="assets/protocols/[^\"]+\.svg">Mở sơ đồ lớn</a></p>', "", body)
    # Navigation between rendered guides remains local. Source file links stay
    # relative to docs/ so they can still be opened in the user's editor.
    body = body.replace('href="protocol-architecture-guide.html"', 'href="#top"')
    css = """
* { box-sizing: border-box; }
html { scroll-behavior: smooth; }
body { margin: 0; background: #f3f5f8; color: #203046; font: 17px/1.8 'Segoe UI',Arial,sans-serif; }
main { max-width: 1080px; margin: 28px auto; padding: 44px 56px; background: white; }
h1 { font-size: 32px; line-height: 1.35; color: #10263d; }
h2 { margin-top: 64px; padding-top: 12px; border-top: 2px solid #e5edf5; font-size: 25px; line-height: 1.4; }
h3 { margin-top: 34px; font-size: 20px; color: #155e75; }
a { color: #145bc4; text-underline-offset: 3px; }
code { font: .9em/1.65 Consolas,monospace; background: #eef3f8; padding: 2px 5px; border-radius: 3px; }
pre { white-space: pre; overflow: auto; padding: 20px; background: #eef3f8; }
pre code { padding: 0; background: none; }
table { border-collapse: collapse; width: 100%; margin: 22px 0; font-size: 15px; }
th,td { padding: 12px; text-align: left; vertical-align: top; border-bottom: 1px solid #dce4ed; overflow-wrap: anywhere; }
th { background: #edf3fa; }
figure { margin: 28px 0; }
.diagram { overflow-x: auto; background: white; }
.diagram svg { width: 100%; min-width: 700px; height: auto; display: block; }
figcaption { text-align: center; font-size: 14px; color: #53677c; }
.reading-note { padding: 16px 20px; background: #edf6ff; border-left: 4px solid #2563eb; }
.back { position: sticky; bottom: 18px; display: block; width: max-content; margin-left: auto; background: white; padding: 6px 12px; border: 1px solid #dce4ed; }
@media(max-width:760px) { body { font-size: 16px; } main { margin: 0; padding: 22px 18px; } h1 { font-size: 27px; } table { display: block; overflow-x: auto; } th,td { min-width: 145px; } }
@media print { body { background: white; font-size: 11pt; } main { margin: 0; max-width: none; padding: 0; } .back,.reading-note { display:none; } .diagram svg { min-width: 0; max-height: 245mm; } figure, tr { break-inside: avoid; } }
"""
    document = ("<!doctype html>\n<html lang=\"vi\"><head><meta charset=\"utf-8\">"
                '<meta name="viewport" content="width=device-width,initial-scale=1">'
                '<title>Học kiến trúc giao thức STM32F334</title>'
                f"<style>{css}</style></head><body><main id=\"top\">"
                '<p class="reading-note">Bản đọc trực tiếp: sơ đồ đã vẽ sẵn. '
                'Dùng mục lục để chọn phần; không cần tiện ích hay kết nối mạng.</p>'
                + body + '<a class="back" href="#top">↑ Mục lục</a></main></body></html>\n')
    DOC.with_suffix(".html").write_text(document, encoding="utf-8", newline="\n")
    print(f"Rendered {len(rendered)} PNG/SVG diagrams and {DOC.with_suffix('.html').name}")


if __name__ == "__main__":
    main()
