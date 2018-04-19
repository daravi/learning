//'default_nettype none
/*
Instance template:
task_2b T2B
(   
   .clk( CLOCK_50 ),
   .on_sig( ),
   .q_s(),
   .addr_s(),
   .data_s(),
   .wren_s(),
   .q_d(),
   .addr_d(),
   .data_d(),
   .wren_d(),
   .q_e(),
   .addr_e(), 
   .finished_sig()
);
*/

module task_2b
#(parameter message_length = 2'b32)
(
   input  logic        clk,    // Clock
   input  logic        on_sig, // Synchronous active high reset
   input  logic [7:0]  q_s,
   output logic [7:0]  addr_s,
   output logic [7:0]  data_s,
   output logic        wren_s,
   input  logic [7:0]  q_d,   
   output logic [7:0]  addr_d,
   output logic [7:0]  data_d,
   output logic        wren_d,
   input  logic [7:0]  q_e,   
   output logic [7:0]  addr_e, 
   output logic        finished_sig
);

// state bits
typedef enum logic [4:0] {idle_state, 
                          i_reg,
                          si_rd, si_reg, 
                          j_reg,
                          sj_rd, sj_reg,
                          tmp_reg,
                          si_wr, sj_wr,
                          f_rd, f_reg,
                          ek_rd, ek_reg, dk_wr,
                          fin_reg, k_reg,
                          finished_state} statetype;

statetype state = idle_state;


// wires
logic increment_bit, fin_reg_in, fin_reg_out, started = 1'b0;  
logic [1:0] sk_idx;
logic [4:0] k_reg_in,      k_reg_out;
logic [7:0] i_reg_in,      i_reg_out,
            j_reg_in,      j_reg_out,
            si_reg_in,     si_reg_out,
            sj_reg_in,     sj_reg_out,
            tmp_reg_in,    tmp_reg_out,
            f_reg_in,      f_reg_out,
            ek_reg_in,     ek_reg_out; 


// (state) register
always_ff @(posedge clk, negedge rst)
   if (rst) 
      begin
//       initialize register
         state <= idle_state;
         { i_reg_out,
           j_reg_out,
           k_reg_out,
           si_reg_out,
           sj_reg_out,
           tmp_reg_out,
           f_reg_out,
           ek_reg_out,
           fin_reg_out } <= 0;
      end
   else
      begin
//       reg_out              <= reg_in
         i_reg_out            <= i_reg_in;
         j_reg_out            <= j_reg_in;
         k_reg_out            <= k_reg_in;
         si_reg_out           <= si_reg_in;
         sj_reg_out           <= sj_reg_in;
         tmp_reg_out          <= tmp_reg_in;
         f_reg_out            <= f_reg_in;
         ek_reg_out           <= ek_reg_in;
         fin_reg_out          <= fin_reg_in;


//                      state <= nextstate
         case (state)
            idle_state: state <= i_reg;
//          i = i + 1
            i_reg:      state <= si_rd;
//          j = j + s[i]
            si_rd:      state <= si_reg;
            si_reg:     state <= j_reg;
            j_reg:      state <= sj_rd:;
            // swap values of s[i] and s[j]
            sj_rd:      state <= sj_reg;
            sj_reg:     state <= tmp_reg;
            tmp_reg:    state <= si_wr;
            si_wr:      state <= sj_wr;
            sj_wr:      state <= f_rd;
            // f = s[(s[i] + s[j])]
            f_rd:       state <= f_reg;
            f_reg:      state <= ek_rd;
            // decr'd_out[k] = (f xor encr'd_in[k])
            ek_rd:     state <= ek_reg;
            ek_reg:    state <= dk_wr;
            dk_wr:     state <= fin_reg;
            // increment k
            fin_reg:   state <= k_reg;
            k_reg:      
               begin 
                  if (fin_reg_out)
                       state <= finished_state;
                  else
                       state <= i_reg;
               end

            finished_state:
                        state <= finished_state;
         
            default :   state <= state;
         endcase
      end


// input logic
always_comb

   increment_bit   = ~fin_reg_out;

   case (state)   
      i_reg:
         begin 
            i_reg_in   = i_reg_out + 8'b1;
            j_reg_in   = j_reg_out;
            k_reg_in   = k_reg_out;
            si_reg_in  = si_reg_out;
            sj_reg_in  = sj_reg_out;
            tmp_reg_in = tmp_reg_out;
            f_reg_in   = f_reg_out;
            ek_reg_in  = ek_reg_out;
            fin_reg_in = fin_reg_out;
         end
      si_reg:
         begin 
            i_reg_in   = i_reg_out;
            j_reg_in   = j_reg_out;
            k_reg_in   = k_reg_out;
            si_reg_in  = q_s;
            sj_reg_in  = sj_reg_out;
            tmp_reg_in = tmp_reg_out;
            f_reg_in   = f_reg_out;
            ek_reg_in  = ek_reg_out;
            fin_reg_in = fin_reg_out;
         end
      j_reg:
         begin 
            i_reg_in   = i_reg_out;
            j_reg_in   = j_reg_out + si_reg_out;
            k_reg_in   = k_reg_out;
            si_reg_in  = si_reg_out;
            sj_reg_in  = sj_reg_out;
            tmp_reg_in = tmp_reg_out;
            f_reg_in   = f_reg_out;
            ek_reg_in  = ek_reg_out;
            fin_reg_in = fin_reg_out;
         end      
      sj_reg:
         begin 
            i_reg_in   = i_reg_out;
            j_reg_in   = j_reg_out;
            k_reg_in   = k_reg_out;
            si_reg_in  = si_reg_out;
            sj_reg_in  = q_s;
            tmp_reg_in = tmp_reg_out;
            f_reg_in   = f_reg_out;
            ek_reg_in  = ek_reg_out;
            fin_reg_in = fin_reg_out;
         end
      tmp_reg: 
         begin 
            i_reg_in   = i_reg_out;
            j_reg_in   = j_reg_out;
            k_reg_in   = k_reg_out;
            si_reg_in  = si_reg_out;
            sj_reg_in  = sj_reg_out;
            tmp_reg_in = sj_reg_out;
            f_reg_in   = f_reg_out;
            ek_reg_in  = ek_reg_out;
            fin_reg_in = fin_reg_out;
         end         
      f_reg:
         begin 
            i_reg_in   = i_reg_out;
            j_reg_in   = j_reg_out;
            k_reg_in   = k_reg_out;
            si_reg_in  = si_reg_out;
            sj_reg_in  = sj_reg_out;
            tmp_reg_in = tmp_reg_out;
            f_reg_in   = q_w;
            ek_reg_in  = ek_reg_out;
            fin_reg_in = fin_reg_out;
         end
      ek_reg:
         begin 
            i_reg_in   = i_reg_out;
            j_reg_in   = j_reg_out;
            k_reg_in   = k_reg_out;
            si_reg_in  = si_reg_out;
            sj_reg_in  = sj_reg_out;
            tmp_reg_in = tmp_reg_out;
            f_reg_in   = f_reg_out;
            ek_reg_in  = q_e;
            fin_reg_in = fin_reg_out;
         end
      fin_reg:
         begin 
            i_reg_in   = i_reg_out;
            j_reg_in   = j_reg_out;
            k_reg_in   = k_reg_out;
            si_reg_in  = si_reg_out;
            sj_reg_in  = sj_reg_out;
            tmp_reg_in = tmp_reg_out;
            f_reg_in   = f_reg_out;
            ek_reg_in  = ek_reg_out;
            fin_reg_in = &k_reg_out; // asserted when k == 31
         end
      k_reg:
         begin 
            i_reg_in   = i_reg_out;
            j_reg_in   = j_reg_out;
            if (fin_reg_out)
               k_reg_in = k_reg_out;
            else
               k_reg_in = k_reg_out + increment_bit; 
            si_reg_in  = si_reg_out;
            sj_reg_in  = sj_reg_out;
            tmp_reg_in = tmp_reg_out;
            f_reg_in   = f_reg_out;
            ek_reg_in  = ek_reg_out;
            fin_reg_in = fin_reg_out;
         end

      default:
         begin 
            i_reg_in   = i_reg_out;
            j_reg_in   = j_reg_out;
            k_reg_in   = k_reg_out;
            si_reg_in  = si_reg_out;
            sj_reg_in  = sj_reg_out;
            tmp_reg_in = tmp_reg_out;
            f_reg_in   = f_reg_out;
            ek_reg_in  = ek_reg_out;
            fin_reg_in = fin_reg_out;
         end
   endcase

/// output logic
always_comb
   begin
      case(state)
         si_rd:
            begin 
               wren_s = 1'b0;
               addr_s = i_reg_out;
               data_s = 8'b0;
               wren_d = 1'b0;
               addr_d = 8'b0;
               data_d = 8'b0;
               addr_e = 8'b0;
            end
         sj_rd:
            begin 
               wren_s = 1'b0;
               addr_s = j_reg_out;
               data_s = 8'b0;
               wren_d = 1'b0;
               addr_d = 8'b0;
               data_d = 8'b0;
               addr_e = 8'b0;
            end
         si_wr:
            begin 
               wren_s = 1'b1;
               addr_s = i_reg_out;
               data_s = sj_reg_out;
               wren_d = 1'b0;
               addr_d = 8'b0;
               data_d = 8'b0;
               addr_e = 8'b0;
            end
         sj_wr:
            begin 
               wren_s = 1;
               addr_s = j_reg_out;
               data_s = tmp_reg_out;
               wren_d = 1'b0;
               addr_d = 8'b0;
               data_d = 8'b0;
               addr_e = 8'b0;
            end
         f_rd:
            begin 
               wren_s = 1'b0;
               addr_s = (si_reg_out + sj_reg_out);
               data_s = 8'b0;
               wren_d = 1'b0;
               addr_d = 8'b0;
               data_d = 8'b0;
               addr_e = 8'b0;
            end
         ek_rd:
            begin 
               wren_s = 1'b0;
               addr_s = 8'b0;
               data_s = 8'b0;
               wren_d = 1'b0;
               addr_d = 8'b0;
               data_d = 8'b0;
               addr_e = k_reg_out;
            end
         dk_wr:
            begin 
               wren_s = 1'b0;
               addr_s = 8'b0;
               data_s = 8'b0;
               wren_d = 1'b1;
               addr_d = k_reg_out;
               data_d = (ek_reg_out ^ f_reg_out);
               addr_e = 8'b0;
            end
         default:
            begin 
               wren_s = 1'b0;
               addr_s = 8'b0;
               data_s = 8'b0;
               wren_d = 1'b0;
               addr_d = 8'b0;
               data_d = 8'b0;
               addr_e = 8'b0;
            end
      endcase
      finished_sig    = fin_reg_out;
   end

endmodule // task_2b

