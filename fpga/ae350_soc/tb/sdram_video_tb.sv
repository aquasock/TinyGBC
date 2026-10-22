// SPDX-License-Identifier: MIT
//
// Checks ahb_sdram's video line fetch against a behavioural SDRAM: a 160
// halfword line arrives with its indexes in order and one vdone, every word
// belongs to the address the line names, and a CPU read through the line
// buffer still works.
//
// This is the half of the picture path the video unit's own testbench cannot
// see. That one models the vstart/vdata channel, so nothing in the tree
// exercised the controller's column loop until core-log entry 8, where the
// loop still read 128 of the line's 160 halfwords and vdone never fired.
//
// The model answers commands where the part sees them, on the rising edge of
// the inverted bus clock, which is clk falling. Run by
// tools/sim_sdram_video.sh.
`timescale 1ns/1ps

// ahb_sdram instantiates the vendor's ODDR for the SDRAM clock; simulation
// only needs the clock it hands out, not its timing.
module ODDR(input wire D0, input wire D1, input wire TX, input wire CLK,
	    output wire Q0, output wire Q1);
	assign Q0 = D0;
	assign Q1 = D1;
endmodule

module sdram_video_tb;
	localparam integer CL = 3;
	localparam [31:0] FB = 32'h01fc0000; // frame buffer 0 within the SDRAM
	// The controller's own view of that address: op_addr is the byte address
	// over 3, and the part's row, bank and column come out of it.
	localparam [24:3] OP = FB[24:3];
	localparam [12:0] ROW = OP[24:12];
	localparam [1:0] BANK = OP[11:10];
	localparam [8:0] COLBASE = {OP[9:8], 7'd0};

	reg clk = 0;
	reg resetn = 0;
	always #5 clk = ~clk;

	// ------------------------------------------------------- the controller
	wire [15:0] dq;
	reg [15:0] dq_drive = 16'd0;
	reg dq_oe = 0;
	assign dq = dq_oe ? dq_drive : 16'bz;

	reg req = 0, write = 0;
	reg [24:3] addr = 24'd0;
	reg [7:0] lanes = 8'hff;
	reg [63:0] wdata = 64'd0;
	wire ack, ready;
	wire [63:0] rdata;

	reg vstart = 0;
	reg [24:8] vaddr = 17'd0;
	wire vvalid, vdone;
	wire [7:0] vindex;
	wire [15:0] vdata;

	wire sdram_clk;
	wire sdram_cs_n, sdram_ras_n, sdram_cas_n, sdram_we_n;
	wire [1:0] sdram_ba, sdram_dqm;
	wire [12:0] sdram_a;

	ahb_sdram #(.CL(CL), .INIT_CYCLES(40), .REFRESH_CYCLES(64)) dut (
		.clk(clk), .resetn(resetn),
		.req(req), .write(write), .addr(addr), .lanes(lanes),
		.wdata(wdata), .ack(ack), .rdata(rdata), .ready(ready),
		.vstart(vstart), .vaddr(vaddr), .vvalid(vvalid),
		.vindex(vindex), .vdata(vdata), .vdone(vdone),
		.sdram_clk(sdram_clk),
		.sdram_cs_n(sdram_cs_n), .sdram_ras_n(sdram_ras_n),
		.sdram_cas_n(sdram_cas_n), .sdram_we_n(sdram_we_n),
		.sdram_ba(sdram_ba), .sdram_a(sdram_a), .sdram_dqm(sdram_dqm),
		.sdram_dq(dq)
	);

	// ------------------------------------------------------- the SDRAM model
	// The contents are a function of the address, so a word says where it
	// came from: bits 15:9 the row's low bits, 8:7 the bank, 6:0 the
	// column's low bits.
	function [15:0] memword(input [12:0] row, input [1:0] bank,
				input [8:0] col);
		memword = {row[6:0], bank, col[6:0]};
	endfunction

	reg active = 0;
	reg [12:0] mrow = 13'd0;
	reg [1:0] mbank = 2'd0;

	// Reads in flight, one per clock, each carrying the word it will return.
	localparam integer MAXR = 8;
	reg [15:0] rd_val [0:MAXR-1];
	reg [MAXR-1:0] rd_valid = 0;
	integer k;

	always @(negedge clk) begin
		for (k = MAXR - 1; k > 0; k = k - 1) begin
			rd_val[k] <= rd_val[k-1];
			rd_valid[k] <= rd_valid[k-1];
		end
		rd_valid[0] <= 1'b0;
		if (!sdram_cs_n) begin
			if (!sdram_ras_n && sdram_cas_n && sdram_we_n) begin
				// ACTIVE
				active <= 1'b1;
				mrow <= sdram_a;
				mbank <= sdram_ba;
			end else if (sdram_ras_n && !sdram_cas_n && sdram_we_n) begin
				// READ: this word returns CL clocks later
				rd_val[0] <= memword(mrow, mbank, sdram_a[8:0]);
				rd_valid[0] <= 1'b1;
			end else if (!sdram_ras_n && sdram_cas_n && !sdram_we_n) begin
				// PRECHARGE
				active <= 1'b0;
			end
		end
	end

	// Stage CL - 1 holds the word a READ issued CL clocks ago, which is where
	// the controller's capture (the falling bus clock edge CL clocks after the
	// command) expects to find it.
	always @(*) begin
		dq_oe = rd_valid[CL-1];
		dq_drive = rd_val[CL-1];
	end

	// ------------------------------------------------------------- the test
	integer checks = 0, failures = 0;
	integer i, got, done_count, last_index;
	reg [15:0] first_word, second_word;
	reg bad_word;

	task check(input condition, input [255:0] what);
		begin
			checks = checks + 1;
			if (!condition) begin
				failures = failures + 1;
				$display("FAIL %0s", what);
			end
		end
	endtask

	initial begin
		resetn = 0;
		repeat (8) @(posedge clk);
		resetn = 1;

		// The controller initialises the part first.
		wait (ready === 1'b1);
		repeat (4) @(posedge clk);

		// A line fetch of line 0 of frame buffer 0.
		@(negedge clk);
		vaddr = FB[24:8];
		vstart = 1'b1;
		@(negedge clk);
		vstart = 1'b0;

		got = 0;
		done_count = 0;
		last_index = -1;
		bad_word = 1'b0;
		// 160 halfwords, at most one per clock, plus the pipeline's depth.
		repeat (400) begin
			@(posedge clk);
			if (vvalid) begin
				if (vindex != got[7:0]) begin
					bad_word = 1'b1;
					if (got < 4)
						$display("index %0d where %0d expected",
							 vindex, got);
				end
				// Word n is the line's column base plus n.
				if (vdata !== memword(ROW, BANK, COLBASE + got[8:0])) begin
					bad_word = 1'b1;
					if (got < 4)
						$display("word %0d = %04x, wanted %04x",
							 got, vdata,
							 memword(ROW, BANK,
								 COLBASE + got[8:0]));
				end
				got = got + 1;
				last_index = vindex;
			end
			if (vdone)
				done_count = done_count + 1;
		end

		check(got == 160, "160 halfwords in the line");
		check(last_index == 159, "the line ends at index 159");
		check(done_count == 1, "one vdone");
		check(!bad_word, "every word is the line's own");

		// A CPU read after the fetch, which the line buffer answers: 8 bytes
		// at the start of line 0 are columns 0 to 3 of row 0.
		addr = FB[24:3];
		write = 1'b0;
		req = 1'b1;
		wait (ack === 1'b1);
		@(posedge clk);
		req = 1'b0;
		@(posedge clk);
		first_word = rdata[15:0];
		second_word = rdata[31:16];
		check(second_word === memword(ROW, BANK, COLBASE + 9'd1),
		      "a CPU read sees its second column");
		check(first_word === memword(ROW, BANK, COLBASE),
		      "a CPU read sees its first column");

		$display("TinyGBC SDRAM SIM checks=%0d failures=%0d halfwords=%0d",
			 checks, failures, got);
		if (failures == 0)
			$display("PASS");
		$finish;
	end

	initial begin
		#200000;
		$display("FAIL timeout");
		$finish;
	end
endmodule
