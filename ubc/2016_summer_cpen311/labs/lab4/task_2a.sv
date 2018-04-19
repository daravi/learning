//'default_nettype none
/*
Instance template:
task_2a T2A
(   
   .clk( CLOCK_50 ),
   .on_sig( ),
   .secret_key(),
   .q_s(),
   .addr_s(),
   .data_s(),
   .wren_s(),
   .finished_sig()
);
*/

module task_2a
#(parameter key_length = 3)
(
   input  logic        clk,    // Clock
   input  logic        on_sig, // Synchronous active high reset
   input  logic [23:0] secret_key,
   input  logic [7:0]  q_s,
   output logic [7:0]  addr_s,
   output logic [7:0]  data_s,
   output logic        wren_s,  
   output logic        finished_sig 
);

// state bits
typedef enum logic [3:0] {idle_state, 
                          si_rd, si_reg, 
                          skb_reg,
                          j_reg, 
                          sj_rd, sj_reg, 
                          tmp_reg, 
                          cpy_sj_wr, cpy_si_wr, 
                          fin_reg, i_reg, 
                          finished_state} statetype;

statetype state = idle_state;


// registers and wires
logic increment_bit, fin_reg_in, fin_reg_out, started = 1'b0;  
logic [1:0] sk_idx;
logic [7:0] i_reg_in,      i_reg_out,
            j_reg_in,      j_reg_out,
            skb_reg_in,    skb_reg_out,
            si_reg_in,     si_reg_out,
            sj_reg_in,     sj_reg_out,
            tmp_reg_in,    tmp_reg_out;

// (state) register
always_ff @(posedge clk)
   if (!(on_sig | started)) 
      begin
//       initialize register
         state <= idle_state;
         { i_reg_out, j_reg_out, 
           skb_reg_out, 
           si_reg_out, sj_reg_out, 
           tmp_reg_out, 
           fin_reg_out }
        <= 0;
      end
   else
      begin
         started              <= 1'b1;

//       reg_out              <= reg_in
         i_reg_out            <= i_reg_in;
         j_reg_out            <= j_reg_in;
         skb_reg_out          <= skb_reg_in;
         si_reg_out           <= si_reg_in;
         sj_reg_out           <= sj_reg_in;
         tmp_reg_out          <= tmp_reg_in;
         fin_reg_out          <= fin_reg_in;

//                      state <= nextstate;   
         case (state)
            idle_state: state <= skb_reg;
//          j = (j + s[i] + secret_key[i mod keylength])
            skb_reg:    state <= si_rd;
            si_rd:      state <= si_reg;
            si_reg:     state <= j_reg;
            j_reg:      state <= sj_rd;
//          swap values of s[i] and s[j]
            sj_rd:      state <= sj_reg;
            sj_reg:     state <= tmp_reg;
            tmp_reg:    state <= cpy_sj_wr;
            cpy_sj_wr:  state <= cpy_si_wr;
            cpy_si_wr:  state <= fin_reg;
//          increment i
            fin_reg:    state <= i_reg;
            i_reg:
               begin
                  if (fin_reg_out)
                        state <= finished_state;
                  else
                        state <= skb_reg;
               end

            finished_state: 
                        state <= finished_state;

            default:    state <= state;

   endcase
      end


// input logic
always_comb
   begin
   
   sk_idx = (i_reg_out % key_length);
   
   case (sk_idx)
      2'b10:   skb_reg_in  = secret_key[7:0];
      2'b01:   skb_reg_in  = secret_key[15:8];
      2'b00:   skb_reg_in  = secret_key[23:16];
      default: skb_reg_in  = secret_key[7:0];
   endcase
   
   increment_bit   = ~fin_reg_out;
   
   case (state)
      j_reg:
         begin
            i_reg_in   = i_reg_out;
            j_reg_in   = (j_reg_out + si_reg_out + skb_reg_out);
            si_reg_in  = si_reg_out;
            sj_reg_in  = sj_reg_out;
            tmp_reg_in = si_reg_out;
            fin_reg_in = fin_reg_out;
         end
      si_reg:
         begin
            i_reg_in   = i_reg_out;
            j_reg_in   = j_reg_out;
            si_reg_in  = q_s;
            sj_reg_in  = sj_reg_out;
            tmp_reg_in = si_reg_out;
            fin_reg_in = fin_reg_out;
         end
      sj_reg:
         begin
            i_reg_in   = i_reg_out;
            j_reg_in   = j_reg_out;
            si_reg_in  = si_reg_out;
            sj_reg_in  = q_s;
            tmp_reg_in = si_reg_out;
            fin_reg_in = fin_reg_out;
         end
      tmp_reg:
         begin
            i_reg_in   = i_reg_out;
            j_reg_in   = j_reg_out;
            si_reg_in  = si_reg_out;
            sj_reg_in  = sj_reg_out;
            tmp_reg_in = si_reg_out;
            fin_reg_in = fin_reg_out;
         end
      fin_reg:
         begin
            i_reg_in   = i_reg_out;
            j_reg_in   = j_reg_out;
            si_reg_in  = si_reg_out;
            sj_reg_in  = sj_reg_out;
            tmp_reg_in = tmp_reg_out;
            fin_reg_in = &i_reg_out; // asserted when i == 255
         end
      i_reg:
         begin
            if (fin_reg_out)
               i_reg_in = i_reg_out;
            else
               i_reg_in = i_reg_out + increment_bit;
            j_reg_in   = j_reg_out;
            si_reg_in  = si_reg_out;
            sj_reg_in  = sj_reg_out;
            tmp_reg_in = tmp_reg_out;
            fin_reg_in = fin_reg_out;
         end
      
      default:
         begin
            i_reg_in   = i_reg_out;
            j_reg_in   = j_reg_out;
            si_reg_in  = si_reg_out;
            sj_reg_in  = sj_reg_out;
            tmp_reg_in = tmp_reg_out;
            fin_reg_in = fin_reg_out;
         end
   endcase
   end


// output logic
always_comb
   begin
      case(state)
         idle_state:
            begin 
               wren_s       = 1'b0;
               addr_s       = 1'b0;
               data_s       = 1'b0;
            end
         si_rd:
            begin
               wren_s       = 1'b0;
               addr_s       = i_reg_out;
               data_s       = 1'b0;
            end
         sj_rd:
            begin
               wren_s       = 1'b0;
               addr_s       = j_reg_out;
               data_s       = 1'b0;
            end
         cpy_sj_wr:
            begin
               wren_s       = 1'b1;
               addr_s       = i_reg_out;
               data_s       = sj_reg_out;
            end
         cpy_si_wr:
            begin
               wren_s       = 1'b1;
               addr_s       = j_reg_out;
               data_s       = tmp_reg_out;
            end
         default:
            begin
               wren_s       = 1'b0;
               addr_s       = 1'b0;
               data_s       = 1'b0;
            end
      endcase
      finished_sig = fin_reg_out;
   end

endmodule // task_2a

   

