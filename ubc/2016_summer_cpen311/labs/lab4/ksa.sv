module ksa
(   
   //////////// CLOCK //////////
   input                       CLOCK_50,

   //////////// LED //////////
   output           [8:0]      LEDG,
   output          [17:0]      LEDR,

   //////////// KEY //////////
   input            [3:0]      KEY,

   //////////// SW //////////
   input           [17:0]      SW,

   //////////// SEG7 //////////
   output           [6:0]      HEX0,
   output           [6:0]      HEX1,
   output           [6:0]      HEX2,
   output           [6:0]      HEX3,
   output           [6:0]      HEX4,
   output           [6:0]      HEX5,
   output           [6:0]      HEX6,
   output           [6:0]      HEX7
);

//=======================================================
//  REG/WIRE declarations
//=======================================================
   logic clk, rst, run_t1, run_t2a, run_t2b, wren_s, wren_d, wren_d_t2b, finished_t1, finished_t2a, finished_t2b, 
		   wren_s_t1, wren_s_t2a, wren_s_t2b;
   logic [7:0] addr_s, data_s, s_sig_t1, q_s, q_e, q_d, addr_d, data_d, addr_e, 
	            addr_s_t2a, data_s_t2a, addr_s_t2b, data_s_t2b, addr_d_t2b, data_d_t2b, addr_e_t2b;
	logic [23:0] key, key2;
   
   assign clk   = CLOCK_50;
   assign rst = KEY[3];
   assign key = 24'h000249;
   
//=======================================================
//  Modules
//=======================================================

   s_memory s_memory_inst (
		.address ( addr_s ),
		.clock   ( clk ),
		.data    ( data_s ),
		.wren    ( wren_s ),
		.q       ( q_s )
		);
	
	encrypted_rom	encrypted_rom_inst (
		.address ( addr_e ),
		.clock   ( clk ),
		.q       ( q_e )
		);
	
	decrypted_output	decrypted_output_inst (
		.address ( addr_d ),
		.clock   ( clk ),
		.data    ( data_d ),
		.wren    ( wren_d ),
		.q       ( q_d )
		);
	
   
   MemoryInitializer MI1 (  
		.clk          ( clk ),
		.mem_sig      ( s_sig_t1 ),
		.wren         ( wren_s_t1 ),
		.on_sig       ( run_t1 ),
		.finished_sig ( finished_t1 )
		);
   
   task_2a T2A (   
		.clk( clk ),
		.on_sig( run_t2a ),
		.secret_key( key ),
		.q_s( q_s ),
		.addr_s( addr_s_t2a ),
		.data_s( data_s_t2a ),
		.wren_s( wren_s_t2a ),
		.finished_sig( finished_t2a )
		);


	task_2b T2B (   
   .clk( CLOCK_50 ),
   .on_sig( run_t2b ),
   .q_s( q_s ),
   .addr_s( addr_s_t2b ),
   .data_s( data_s_t2b ),
   .wren_s( wren_s_t2b ),
   .q_d( q_d ),
   .addr_d( addr_d_t2b ),
   .data_d( data_d_t2b ),
   .wren_d( wren_d_t2b ),
   .q_e( q_e ),
   .addr_e( addr_e_t2b ), 
   .finished_sig( finished_t2b )
	);

   
//=======================================================
//  Main FSM
//=======================================================


   // state bits
   typedef enum logic [3:0] {idle, start,
									  task_1, 
                             task_2a, task_2b,
                             finish} statetype;

   statetype state = idle;

   // register ins/outs
logic started = 1'b0;
   // (state) register
   always_ff @(posedge clk)
      if (!(rst | started))
         begin 
            state       <= start;
				started = 1'b1;
            // { register } <= 0;
         end
      else
		 // reg out       <= reg in
		case(state)
		
			idle:  state <= idle;
			start: state <= task_1;
			task_1:
				begin 
					if (finished_t1)
                  state <= task_2a;
               else
                  state <= task_1;
				end
			task_2a:
				begin
					if (finished_t2a)
                  state <= task_2b;
               else
                  state <= task_2a;
				end
			task_2b:
				begin
					if (finished_t2b)
                  state <= finish;
               else
                  state <= task_2b;
				end
			finish:
				begin
				   state    <= finish; // ??
				end
		endcase


   // output logic
   always_comb
		case(state)
			start:
				begin
					run_t1       = 1'b0;
					run_t2a      = 1'b0;
					run_t2b      = 1'b0;
					wren_s       = 1'b0;
					addr_s       = 1'b0;
					data_s       = 1'b0;
					wren_d       = 1'b0;
					addr_d       = 1'b0;
					data_d       = 1'b0;
					addr_e       = 1'b0;
					LEDG[3:0]	 = 4'b1111;
				end
			task_1:
				begin
					run_t1       = 1'b1;
					run_t2a      = 1'b0;
					run_t2b      = 1'b0;
					wren_s       = wren_s_t1;
					addr_s       = s_sig_t1;
					data_s       = s_sig_t1;
					wren_d       = 1'b0;
					addr_d       = 1'b0;
					data_d       = 1'b0;
					addr_e       = 1'b0;
					LEDG[3:0]	 = 4'b0001;
				end
			task_2a:
				begin
					run_t1       = 1'b0;
					run_t2a      = 1'b1;
					run_t2b      = 1'b0;
					wren_s       = wren_s_t2a;
					addr_s       = addr_s_t2a;
					data_s       = data_s_t2a;
					wren_d       = 1'b0;
					addr_d       = 1'b0;
					data_d       = 1'b0;
					addr_e       = 1'b0;
					LEDG[3:0]	 = 4'b0010;
				end
			task_2b:
				begin
					run_t1       = 1'b0;
					run_t2a      = 1'b0;
					run_t2b      = 1'b1;
					wren_s       = wren_s_t2b;
					addr_s       = addr_s_t2b;
					data_s       = data_s_t2b;
					wren_d       = wren_d_t2b;
					addr_d       = addr_d_t2b;
					data_d       = data_d_t2b;
					addr_e       = addr_e_t2b;
					LEDG[3:0]	 = 4'b0100;
				end
			finish:
				begin
					run_t1       = 1'b0;
					run_t2a      = 1'b0;
					run_t2b      = 1'b0;
					wren_s       = 1'b0;
					addr_s       = 1'b0;
					data_s       = 1'b0;
					wren_d       = 1'b0;
					addr_d       = 1'b0;
					data_d       = 1'b0;
					addr_e       = 1'b0;
					LEDG[3:0]	 = 4'b1000;
				end
			default:
				begin
					run_t1       = 1'b0;
					run_t2a      = 1'b0;
					run_t2b      = 1'b0;
					wren_s       = 1'b0;
					addr_s       = 1'b0;
					data_s       = 1'b0;
					wren_d       = 1'b0;
					addr_d       = 1'b0;
					data_d       = 1'b0;
					addr_e       = 1'b0;
					LEDG[3:0]	 = 4'b0000;
				end
		endcase

endmodule // ksa
