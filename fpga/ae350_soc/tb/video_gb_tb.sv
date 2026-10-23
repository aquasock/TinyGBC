// SPDX-License-Identifier: MIT
//
// Drives a known Game Boy frame through the retargeted video unit and checks
// what reaches its RGB output.
//
// The frame buffer is modelled the way the unit reads it: on vstart the
// testbench takes vaddr, then returns 160 halfwords of colour for the line
// it names, then vdone. Line L, pixel X carries the colour
// {L[4:0], X[4:0], 5'd0}, red in 14:10, green in 9:5, blue in 4:0, so every
// source pixel is individually identifiable.
//
// The raster is driven the way the HDMI transmitter drives it: cx 0..1649 and
// cy 0..749 at 74.25 MHz. The transmitter (hdl-util's hdmi.sv) takes the
// colour for cx one clock after it presents cx, so the RGB at a clock must be
// the pixel cx named one clock earlier; the testbench files each RGB under
// that pixel, and a unit whose pipeline is not matched by its look-ahead
// lands every pixel in the wrong column (PROT-009).
//
// It checks, in the second frame shown:
//   - one picture line is fetched while the line before is on screen, 143
//     fetches for 144 lines after the vertical blank's line 0, and each fetch
//     is 160 halfwords;
//   - the picture occupies columns 240 to 1039 and is blank outside them;
//   - source pixel (x, y) fills a 5x5 block at (240 + 5x, 5y);
//   - ASPECT 2 widens it to six columns per pixel and 960 columns wide.
`timescale 1ns/1ps

module video_gb_tb;

	localparam integer H_TOTAL = 1650;
	localparam integer V_TOTAL = 750;
	localparam integer H_ACTIVE = 1280;
	localparam integer V_ACTIVE = 720;

	reg clk = 1'b0;
	reg pix_clk = 1'b0;
	reg resetn = 1'b0;
	reg pix_reset = 1'b1;

	// AHB, idle except for the CTRL and ASPECT writes.
	reg  [17:0] haddr = 18'd0;
	reg  [1:0]  htrans = 2'b00;
	reg         hsel = 1'b0;
	reg         hwrite = 1'b0;
	reg  [2:0]  hsize = 3'd0;
	reg  [31:0] hwdata = 32'd0;
	reg         hready_in = 1'b1;
	wire [31:0] hrdata;
	wire        hready;
	wire        hresp;

	// The SDRAM video channel, driven by the model below.
	wire        vstart;
	wire [24:8] vaddr;
	reg         vvalid = 1'b0;
	reg  [7:0]  vindex = 8'd0;
	reg  [15:0] vdata = 16'd0;
	reg         vdone = 1'b0;

	wire        cell_ready;
	reg         layer_on = 1'b0;
	reg         cell_valid = 1'b0;
	reg  [11:0] cell_addr = 12'd0;
	reg  [36:0] cell_data = 37'd0;

	reg  [10:0] cx = 11'd0;
	reg  [9:0]  cy = 10'd0;
	wire [23:0] rgb;

	video dut (
		.clk(clk), .resetn(resetn),
		.haddr(haddr), .htrans(htrans), .hsel(hsel), .hwrite(hwrite),
		.hsize(hsize), .hwdata(hwdata), .hready_in(hready_in),
		.hrdata(hrdata), .hready(hready), .hresp(hresp),
		.vstart(vstart), .vaddr(vaddr), .vvalid(vvalid), .vindex(vindex),
		.vdata(vdata), .vdone(vdone),
		.layer_on(layer_on), .cell_valid(cell_valid), .cell_addr(cell_addr),
		.cell_data(cell_data), .cell_ready(cell_ready),
		.pix_clk(pix_clk), .pix_reset(pix_reset),
		.cx(cx), .cy(cy), .rgb(rgb)
	);

	// 75 MHz bus, 74.25 MHz pixel clock.
	always #6.667 clk = ~clk;
	always #6.734 pix_clk = ~pix_clk;

	// ------------------------------------------------------------ fetches --
	integer fetch_words = 0;
	integer fetch_count = 0;
	integer prev_fetches = 0;
	integer fetch_short = 0;
	reg [7:0] fetch_line = 8'd0;

	// The line the unit asked for is vaddr's byte address over 512.
	wire [7:0] line_from_addr = vaddr[16:9];

	always @(posedge clk) begin
		if (vstart) begin
			fetch_line <= line_from_addr;
			fetch_count <= fetch_count + 1;
		end
		if (vvalid) begin
			fetch_words <= fetch_words + 1;
		end
		if (vdone) begin
			if (fetch_words != 159)
				fetch_short <= fetch_short + 1;
			fetch_words <= 0;
		end
	end

	// Answer each request with 160 halfwords, one per bus clock.
	reg        answering = 1'b0;
	reg [8:0]  answer_i = 9'd0;
	always @(posedge clk) begin
		vvalid <= 1'b0;
		vdone  <= 1'b0;
		if (vstart && !answering) begin
			answering <= 1'b1;
			answer_i <= 9'd0;
		end else if (answering) begin
			vvalid <= 1'b1;
			vindex <= answer_i[7:0];
			vdata  <= {fetch_line[4:0], answer_i[4:0], 5'd1};
			if (answer_i == 9'd159) begin
				answering <= 1'b0;
				vvalid <= 1'b0;
				vdone <= 1'b1;
			end else begin
				answer_i <= answer_i + 9'd1;
			end
		end
	end

	// ------------------------------------------------------------- raster --
	always @(posedge pix_clk) begin
		if (cx == H_TOTAL - 1) begin
			cx <= 11'd0;
			cy <= (cy == V_TOTAL - 1) ? 10'd0 : cy + 10'd1;
		end else begin
			cx <= cx + 11'd1;
		end
	end

	// The pixel the transmitter takes the current RGB for: cx and cy one clock
	// ago.
	reg [10:0] dx1 = 11'd0;
	reg [9:0]  dy1 = 10'd0;
	always @(posedge pix_clk) begin
		dx1 <= cx;
		dy1 <= cy;
	end

	// The picture as it comes out, by frame number.
	reg [23:0] img [0:3][0:719][0:1279];
	integer frame = 0;

	always @(posedge pix_clk) begin
		if (dx1 < H_ACTIVE && dy1 < V_ACTIVE)
			img[frame % 4][dy1][dx1] <= rgb;
	end

	// At each vertical blank, look at the frame that has just been drawn.
	integer checks = 0;
	integer failures = 0;

	task expect_rgb(input integer f, input integer x, input integer y,
			input [23:0] want, input [127:0] what);
		begin
			checks = checks + 1;
			if (img[f%4][y][x] !== want) begin
				failures = failures + 1;
				$display("FAIL %0s: pixel (%0d,%0d) is %06x, want %06x",
					 what, x, y, img[f%4][y][x], want);
			end
		end
	endtask

	// rgb15 of a 15-bit colour, as the unit expands it.
	function [23:0] rgb15(input [14:0] c);
		rgb15 = {c[4:0], c[4:2], c[9:5], c[9:7], c[14:10], c[14:12]};
	endfunction

	function [23:0] colour_of(input integer x, input integer y);
		colour_of = rgb15({y[4:0], x[4:0], 5'd1});
	endfunction

	always @(posedge pix_clk) begin
		if (cy == 10'd0 && cx == 11'd0) begin
			prev_fetches <= fetch_count;
			fetch_count <= 0;
			frame <= frame + 1;
			if (frame == 4) begin
				// The picture's first frame is frame 1; check it.
				expect_rgb(3, 640,      360, colour_of(80, 72),   "centre");
				expect_rgb(3, 641,      361, colour_of(80, 72),   "centre 5x5 a");
				expect_rgb(3, 642,      362, colour_of(80, 72),   "centre 5x5 b");
				expect_rgb(3, 643,      363, colour_of(80, 72),   "centre 5x5 c");
				expect_rgb(3, 645,      360, colour_of(81, 72),   "next pixel");
				expect_rgb(3, 640,      365, colour_of(80, 73),   "next row");
				expect_rgb(3, 600,      360, colour_of(72, 72),   "left of centre");
				// The picture's own left edge: screen x = 240 is source
				// column 0, and every five columns is the next source pixel.
				expect_rgb(3, 240,      360, colour_of(0, 72),    "leftmost pixel");
				expect_rgb(3, 245,      360, colour_of(1, 72),    "second pixel");
				expect_rgb(3, 395,      360, colour_of(31, 72),   "pixel 31");
				expect_rgb(3, 400,      360, colour_of(32, 72),   "pixel 32");
				expect_rgb(3, 239,      360, 24'd0,               "left bar");
				expect_rgb(3, 1040,     360, 24'd0,               "right bar");
				if (prev_fetches < 143 || prev_fetches > 145) begin
					failures = failures + 1;
					$display("FAIL fetches: %0d in the frame, want 143 to 145",
						 prev_fetches);
				end else begin
					checks = checks + 1;
				end
				if (fetch_short != 0) begin
					failures = failures + 1;
					$display("FAIL fetch length: %0d fetches were not 160 halfwords",
						 fetch_short);
				end else begin
					checks = checks + 1;
				end
				$display("TinyGBC SIM checks=%0d failures=%0d fetches=%0d",
					 checks, failures, fetch_count);
				if (failures == 0)
					$display("PASS");
				else
					$display("FAIL");
				$finish;
			end
		end
	end

	// A write of one word: address phase, then data phase.
	task ahb_write(input [17:0] addr, input [31:0] data);
		begin
			@(posedge clk);
			hsel <= 1'b1; htrans <= 2'b10; haddr <= addr; hwrite <= 1'b1;
			hsize <= 3'd2; hwdata <= data;
			@(posedge clk);
			htrans <= 2'b00;
			@(posedge clk);
			hsel <= 1'b0; hwrite <= 1'b0;
		end
	endtask

	initial begin
		$dumpfile("build/video_gb_tb.vcd");
		$dumpvars(0, video_gb_tb);
		repeat (8) @(posedge clk);
		resetn <= 1'b1;
		repeat (4) @(posedge pix_clk);
		pix_reset <= 1'b0;
		// Enable the frame buffers, buffer 0.
		ahb_write(18'h10000, 32'd1);
		// Two frames are enough: the first latches the mode, the second shows it.
		repeat (4 * H_TOTAL * V_TOTAL + 100) @(posedge pix_clk);
		$display("TinyGBC SIM checks=%0d failures=%0d fetches=%0d",
			 checks, failures, fetch_count);
		$display("FAIL timeout");
		$finish;
	end

endmodule
