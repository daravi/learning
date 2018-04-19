//'default_nettype none
/*
>>>> Verilog Instance <<<<
MemoryInitializer MI1
(	
   .clock        ( CLOCK_50 ),
	.rst          ( reset ),
	.mem_sig      ( init_to_s_mem ),
	.wren         ( wren_s ),
	.finished_sig ( finished_t1 )
);
>>>> VHDL <<<<
>> Component Declaration <<
component MemoryInitializer
  port(clock:        in  STD_LOGIC;
	    rst: 		   in  STD_LOGIC;
		 mem_sig:      out STD_LOGIC_VECTOR(7 downto 0);
	    wren:         out STD_LOGIC;
		 finished_sig: out STD_LOGIC);
end component;
>> Instance <<
MI1: MemoryInitializer port map(d0, d1, s(0), low);
*/

module MemoryInitializer
(	
	input  logic clock,
	input  logic rst, 						  // active low
	output logic [7:0] mem_sig = 8'b0, // memory location and data value
	
	output logic wren,          // will be set to 0 once initialization is over
	output logic finished_sig = 1'b0
);

// registers ins/outs
logic increment_bit, finished_reg_in, finished_reg_out;		
logic [7:0] mem_sig_reg_in, mem_sig_reg_out;					  


// registers 
always_ff @(posedge clock, posedge rst)
	if (rst)
		mem_sig_reg_out <= 0;
	else
		begin
			finished_reg_out <= finished_reg_in;
			mem_sig_reg_out  <= mem_sig_reg_in;
		end

// input logic
always_comb
	begin
		finished_reg_in = &mem_sig_reg_out;
		if (finished_reg_out)
			increment_bit = 0;
		else
			increment_bit = 1;
		mem_sig_reg_in = mem_sig_reg_out + increment_bit;
	end

// output connections
always_comb
	begin
		mem_sig = mem_sig_reg_out;
		wren = increment_bit;
		finished_sig = finished_reg_out;
	end

endmodule
			
