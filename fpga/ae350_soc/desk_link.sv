// SPDX-License-Identifier: GPL-2.0-only
// The FPGA UART's link mode: TinyNES speaking TinyTang's frame protocol on
// the line the boot ROM loads programs over, so that TinyTang can draw
// TinyDesk over a running game (TinyTang's desk layer, record PROT-010 in
// .ai/core-reference.md). Frames both ways are
//
//     0xAA len_hi len_lo type payload[len-1]
//
// with the length big-endian, counting the type byte, and below 2048 (a
// length high byte of 8 or more sends the parser back to hunting for 0xAA).
//
// Received, in link mode only (exts_uart passes each byte here instead of
// to its receive buffer):
//   0x01          the core ID; answered 0x01 CORE_ID.
//   0x08 b        the overlay, bit 0 of b.
//   0x13 x y      the layer's write cursor, cell (x, y) of 80x45.
//   0x14 cells    5 bytes a cell from the cursor, which advances across the
//                 grid: the character's low 7 bits, then the foreground and
//                 the background, each a big-endian 15-bit {b, g, r} of five
//                 bits each. A cell past the last is dropped.
//   0x15 b        the layer's enable, bit 0 of b.
//   0x09 h1 h2    TinyTang's Bluetooth pads, two big-endian words in
//                 TinyTang's pad order (tang_pad.h): B 0x001, Y 0x002, SELECT
//                 0x004, START 0x008, up 0x010, down 0x020, left 0x040, right
//                 0x080, A 0x100, X 0x200, L 0x400, R 0x800 and HOME 0x1000,
//                 active high, given to the CPU as pads (h2 in bits 31:16).
//                 TinyTang sends them every 40 ms or so while a button is held
//                 and once when all are released, so both are let go after
//                 HOLD_CLOCKS without one, or when link mode ends, and a
//                 button can never stay held because the frames stopped. A
//                 frame shorter than four bytes changes nothing.
// Every other type is skipped by its length. The layer is shown while the
// link, the overlay and the enable are all on (layer_on); leaving link mode
// clears the overlay, the enable and the cursor.
//
// Sent, a whole frame at a time, through exts_uart's transmitter:
//   0x01 CORE_ID  when asked, first.
//   0x03 j1 j2    the joypad report, two big-endian words, at a report tick
//                 (every REPORT_CLOCKS) when j1 has changed and at least
//                 every REFRESH_REPORTS ticks, in link mode only. j1 is Wii
//                 controller 1 as TinyTang's pad word (tang_pad.h): up 0x010,
//                 down 0x020, left 0x040, right 0x080 from the D-pad, A 0x100
//                 from the Classic's B and B 0x001 from its Y; and LB 0x400
//                 for TAP_REPORTS ticks after a tap of HOME, HOME pressed and
//                 released with no other button pressed meanwhile. TinyTang
//                 flips its overlay on LB's rising edge, reading the newest
//                 report every 40 ms or so, so LB is held long enough for one
//                 of its reads to see it. j2 is 0.
//   0x21 r0 r1 r2 r3 r6 r7
//                 Wii controller 1's last format-3 report as it sent it: the
//                 four 8-bit stick axes LX, RX, LY, RY, then the two button
//                 bytes, active low (wii_pads.sv), at every report tick while
//                 a Classic Controller is present and has reported, in link
//                 mode only, so TinyTang can drive TinyDesk with it as it
//                 does with the pad it reads itself. Its absence for a while
//                 tells TinyTang there is none.
//   TEXT_TYPE     the CPU's text: bytes it writes in link mode are kept until
//                 a line feed, TEXT_MAX bytes, TEXT_IDLE_CLOCKS without one, or
//                 a flush, then sent as one frame. While a frame of text is
//                 waiting or being sent the CPU's writes wait.
// idle is set when nothing is buffered, waiting or being sent.
module desk_link #(
    parameter [7:0] CORE_ID = 8'h4e,          // "N"
    parameter [7:0] TEXT_TYPE = 8'h20,
    parameter integer REPORT_CLOCKS = 1500000, // 20 ms at 75 MHz
    parameter integer REFRESH_REPORTS = 25,
    parameter integer TAP_REPORTS = 13,
    parameter integer TEXT_MAX = 128,
    parameter integer TEXT_IDLE_CLOCKS = 150000, // 2 ms
    parameter integer HOLD_CLOCKS = 18750000     // 250 ms
) (
    input  wire        clk,
    input  wire        resetn,
    input  wire        link,

    input  wire        rx_valid,   // one received byte
    input  wire [7:0]  rx_data,

    input  wire        txt_valid,  // the CPU's byte, taken when txt_ready
    input  wire [7:0]  txt_data,
    output wire        txt_ready,
    input  wire        flush,      // send buffered text now

    output wire        ln_valid,   // to the transmitter's holding register
    output wire [7:0]  ln_data,
    input  wire        ln_ready,
    output wire        idle,

    // Classic Controller 1's buttons, report bytes 4 and 5 (byte 4 in bits
    // 7:0), active high, or 0 when no Classic Controller is present.
    input  wire [15:0] pad_held,
    // The same controller's last format-3 report (wii_pads.sv pad0_report):
    // bytes 0 to 3 in bits 31:0, byte 0 in 7:0, bytes 6 and 7 in 47:32, and
    // whether it is a present Classic Controller that has reported.
    input  wire [47:0] wii_report,
    input  wire        wii_valid,

    // One cell write to the layer: held until cell_ready.
    output reg         cell_valid,
    output reg  [11:0] cell_addr,  // 80 * y + x
    output reg  [36:0] cell_data,  // {character, foreground, background}
    input  wire        cell_ready,
    output wire        layer_on,
    // Bit 0 layer_on, bit 1 the overlay, bit 2 the enable, bit 3 a cell
    // was lost (sticky), bits 15:8 frames received, bits 31:16 cells
    // written (both wrap).
    output wire [31:0] status,
    // TinyTang's Bluetooth pads from the last 0x09 frame, {h2, h1}, or 0.
    output wire [31:0] pads
);
    localparam integer COLS = 80, ROWS = 45, CELLS = COLS * ROWS;
    localparam integer TA = $clog2(TEXT_MAX);

    // Classic Controller buttons in pad_held (WII-001).
    localparam integer W_RIGHT = 7, W_DOWN = 6, W_HOME = 3, W_UP = 8,
                       W_LEFT = 9, W_Y = 13, W_B = 14;

    // ------------------------------------------------------------ parser --
    localparam [2:0] P_MAGIC = 3'd0, P_LEN_HI = 3'd1, P_LEN_LO = 3'd2,
                     P_TYPE = 3'd3, P_BODY = 3'd4;
    reg [2:0] p = P_MAGIC;
    reg [2:0] len_hi = 3'd0;
    reg [10:0] left = 11'd0;     // payload bytes still to come
    reg [7:0] ftype = 8'd0;
    reg [2:0] nth = 3'd0;        // byte within a cell, or within 0x13
    reg [7:0] b0 = 8'd0, b1 = 8'd0, b2 = 8'd0, b3 = 8'd0;
    reg [11:0] cursor = 12'd0;
    reg overlay = 1'b0, enable = 1'b0, lost = 1'b0;
    reg [7:0] frames = 8'd0;
    reg [15:0] cells = 16'd0;
    reg id_req = 1'b0;
    reg [31:0] pads_q = 32'd0;
    reg [$clog2(HOLD_CLOCKS)-1:0] hold = 0;   // clocks the pads are kept for

    assign layer_on = link && overlay && enable;
    assign pads = pads_q;
    assign status = {cells, frames, 4'd0, lost, enable, overlay, layer_on};

    always @(posedge clk or negedge resetn) begin
        if (!resetn) begin
            p <= P_MAGIC;
            len_hi <= 3'd0;
            left <= 11'd0;
            ftype <= 8'd0;
            nth <= 3'd0;
            cursor <= 12'd0;
            overlay <= 1'b0;
            enable <= 1'b0;
            lost <= 1'b0;
            frames <= 8'd0;
            cells <= 16'd0;
            cell_valid <= 1'b0;
            cell_addr <= 12'd0;
            cell_data <= 37'd0;
            pads_q <= 32'd0;
            hold <= 0;
        end else begin
            if (cell_valid && cell_ready)
                cell_valid <= 1'b0;
            if (hold != 0)
                hold <= hold - 1'b1;
            else
                pads_q <= 32'd0;
            if (!link) begin
                p <= P_MAGIC;
                overlay <= 1'b0;
                enable <= 1'b0;
                cursor <= 12'd0;
                pads_q <= 32'd0;
                hold <= 0;
            end else if (rx_valid) begin
                case (p)
                    P_MAGIC:
                        if (rx_data == 8'haa)
                            p <= P_LEN_HI;
                    P_LEN_HI: begin
                        len_hi <= rx_data[2:0];
                        p <= rx_data < 8'd8 ? P_LEN_LO : P_MAGIC;
                    end
                    P_LEN_LO: begin
                        left <= {len_hi, rx_data} - 1'b1;
                        p <= {len_hi, rx_data} == 11'd0 ? P_MAGIC : P_TYPE;
                    end
                    P_TYPE: begin
                        ftype <= rx_data;
                        nth <= 3'd0;
                        frames <= frames + 1'b1;
                        p <= left == 11'd0 ? P_MAGIC : P_BODY;
                    end
                    default: begin   // P_BODY
                        left <= left - 1'b1;
                        if (left == 11'd1)
                            p <= P_MAGIC;
                        nth <= nth == 3'd4 ? 3'd0 : nth + 1'b1;
                        case (nth)
                            3'd0: b0 <= rx_data;
                            3'd1: b1 <= rx_data;
                            3'd2: b2 <= rx_data;
                            3'd3: b3 <= rx_data;
                            default: ;
                        endcase
                        case (ftype)
                            8'h08: if (nth == 3'd0) overlay <= rx_data[0];
                            8'h15: if (nth == 3'd0) enable <= rx_data[0];
                            8'h09:
                                if (nth == 3'd3) begin
                                    pads_q <= {b2, rx_data, b0, b1};
                                    hold <= HOLD_CLOCKS - 1;
                                end
                            8'h13:
                                // b0 holds x until y arrives; a cell off the
                                // grid parks the cursor past the last.
                                if (nth == 3'd1)
                                    cursor <= b0 < COLS && rx_data < ROWS ?
                                              {rx_data[5:0], 6'd0} + {2'd0, rx_data[5:0], 4'd0} + b0 :
                                              CELLS;
                            8'h14:
                                if (nth == 3'd4) begin
                                    if (cursor < CELLS) begin
                                        if (cell_valid && !cell_ready)
                                            lost <= 1'b1;
                                        cell_valid <= 1'b1;
                                        cell_addr <= cursor;
                                        cell_data <= {b0[6:0], b1[6:0], b2, b3[6:0], rx_data};
                                        cells <= cells + 1'b1;
                                        cursor <= cursor + 1'b1;
                                    end
                                end
                            default: ;
                        endcase
                    end
                endcase
            end
        end
    end

    // ------------------------------------------------------ joypad report --
    wire [15:0] j1_now = {5'd0, 1'b0, 1'b0, pad_held[W_B], pad_held[W_RIGHT],
                          pad_held[W_LEFT], pad_held[W_DOWN], pad_held[W_UP],
                          3'd0, pad_held[W_Y]};
    reg home_prev = 1'b0, home_alone = 1'b0;
    reg [7:0] tap = 8'd0;        // report ticks LB has still to be held
    reg [21:0] tick_count = 22'd0;
    reg [7:0] refresh = 8'd0;
    reg [15:0] j1_sent = 16'd0;
    reg pad_req = 1'b0;
    wire [15:0] j1 = j1_now | (tap != 8'd0 ? 16'h0400 : 16'h0000);
    wire tick = tick_count == REPORT_CLOCKS - 1;
    wire others = |(pad_held & ~(16'd1 << W_HOME));

    // ---------------------------------------------------------------- text --
    reg [7:0] txt_mem [0:TEXT_MAX-1];
    reg [TA:0] txt_count = 0;
    reg txt_full = 1'b0;         // a frame of text is waiting or being sent
    reg [17:0] txt_quiet = 18'd0;
    reg [TA-1:0] rd_addr = 0;
    reg [7:0] txt_q = 8'd0;

    assign txt_ready = link && !txt_full;

    always @(posedge clk) begin
        if (txt_valid && txt_ready)
            txt_mem[txt_count[TA-1:0]] <= txt_data;
        txt_q <= txt_mem[rd_addr];
    end

    // -------------------------------------------------------------- sender --
    localparam [1:0] K_ID = 2'd0, K_PAD = 2'd1, K_TEXT = 2'd2, K_WII = 2'd3;
    reg sending = 1'b0;
    reg [1:0] kind = K_ID;
    reg [8:0] idx = 9'd0;        // byte of the frame being sent
    reg [8:0] last = 9'd0;       // its last byte
    reg gap = 1'b0;              // a clock for txt_q to follow rd_addr
    reg [15:0] j1_frame = 16'd0;
    reg wii_req = 1'b0;
    reg [47:0] wii_frame = 48'd0;

    assign ln_valid = sending && !gap;
    assign idle = !sending && !id_req && !pad_req && !wii_req && txt_count == 0;

    reg [7:0] byte_out;
    always @* begin
        if (idx == 9'd0)
            byte_out = 8'haa;
        else if (idx == 9'd1)
            byte_out = 8'h00;
        else if (idx == 9'd2)
            byte_out = last[7:0] - 8'd2;        // the type and the payload
        else case (kind)
            K_ID:  byte_out = idx == 9'd3 ? 8'h01 : CORE_ID;
            K_PAD:
                case (idx)
                    9'd3: byte_out = 8'h03;
                    9'd4: byte_out = j1_frame[15:8];
                    9'd5: byte_out = j1_frame[7:0];
                    default: byte_out = 8'h00;
                endcase
            K_WII:
                case (idx)
                    9'd3: byte_out = 8'h21;
                    9'd4: byte_out = wii_frame[7:0];
                    9'd5: byte_out = wii_frame[15:8];
                    9'd6: byte_out = wii_frame[23:16];
                    9'd7: byte_out = wii_frame[31:24];
                    9'd8: byte_out = wii_frame[39:32];
                    default: byte_out = wii_frame[47:40];
                endcase
            default: byte_out = idx == 9'd3 ? TEXT_TYPE : txt_q;
        endcase
    end
    assign ln_data = byte_out;

    always @(posedge clk or negedge resetn) begin
        if (!resetn) begin
            id_req <= 1'b0;
            home_prev <= 1'b0;
            home_alone <= 1'b0;
            tap <= 8'd0;
            tick_count <= 22'd0;
            refresh <= 8'd0;
            j1_sent <= 16'd0;
            pad_req <= 1'b0;
            txt_count <= 0;
            txt_full <= 1'b0;
            txt_quiet <= 18'd0;
            rd_addr <= 0;
            sending <= 1'b0;
            kind <= K_ID;
            idx <= 9'd0;
            last <= 9'd0;
            gap <= 1'b0;
            j1_frame <= 16'd0;
            wii_req <= 1'b0;
            wii_frame <= 48'd0;
        end else begin
            gap <= 1'b0;

            // The core ID, asked for by a 0x01 frame with no payload.
            if (link && rx_valid && p == P_TYPE && rx_data == 8'h01 && left == 11'd0)
                id_req <= 1'b1;

            tick_count <= tick ? 22'd0 : tick_count + 1'b1;
            if (tick) begin
                if (tap != 8'd0)
                    tap <= tap - 1'b1;
                refresh <= refresh == REFRESH_REPORTS - 1 ? 8'd0 : refresh + 1'b1;
                if (link && (j1 != j1_sent || refresh == REFRESH_REPORTS - 1)) begin
                    pad_req <= 1'b1;
                    j1_sent <= j1;
                end
                if (link && wii_valid)
                    wii_req <= 1'b1;
            end
            if (!link) begin
                pad_req <= 1'b0;
                wii_req <= 1'b0;
            end

            // A tap of HOME, after the tick so that a new tap wins.
            home_prev <= pad_held[W_HOME];
            if (pad_held[W_HOME] && !home_prev)
                home_alone <= !others;
            else if (pad_held[W_HOME] && others)
                home_alone <= 1'b0;
            if (!pad_held[W_HOME] && home_prev && home_alone)
                tap <= TAP_REPORTS;

            // Text from the CPU.
            if (txt_valid && txt_ready) begin
                txt_count <= txt_count + 1'b1;
                txt_quiet <= 18'd0;
                if (txt_data == 8'h0a || txt_count == TEXT_MAX - 1)
                    txt_full <= 1'b1;
            end else if (txt_count != 0 && !txt_full) begin
                txt_quiet <= txt_quiet + 1'b1;
                if (flush || txt_quiet == TEXT_IDLE_CLOCKS - 1)
                    txt_full <= 1'b1;
            end

            // One frame at a time, the ID first, then the pad, the Wii
            // report, then text.
            if (!sending) begin
                idx <= 9'd0;
                if (id_req) begin
                    id_req <= 1'b0;
                    kind <= K_ID;
                    last <= 9'd4;
                    sending <= 1'b1;
                end else if (pad_req) begin
                    pad_req <= 1'b0;
                    kind <= K_PAD;
                    last <= 9'd7;
                    j1_frame <= j1_sent;
                    sending <= 1'b1;
                end else if (wii_req) begin
                    wii_req <= 1'b0;
                    kind <= K_WII;
                    last <= 9'd9;
                    wii_frame <= wii_report;
                    sending <= 1'b1;
                end else if (txt_full) begin
                    kind <= K_TEXT;
                    last <= {1'b0, txt_count} + 9'd3;
                    rd_addr <= 0;
                    sending <= 1'b1;
                end
            end else if (ln_valid && ln_ready) begin
                idx <= idx + 1'b1;
                gap <= 1'b1;
                if (idx >= 9'd3)
                    rd_addr <= idx - 9'd3;
                if (idx == last) begin
                    sending <= 1'b0;
                    if (kind == K_TEXT) begin
                        txt_count <= 0;
                        txt_full <= 1'b0;
                        txt_quiet <= 18'd0;
                    end
                end
            end
        end
    end
endmodule
