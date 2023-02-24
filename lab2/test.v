`timescale 1 ns / 1 ps

	module Capture_Timer_v1_0_S00_AXI
	(
		// Users to add parameters here

		// User parameters ends
		// Do not modify the parameters beyond this line

		// Users to add ports here
        input wire capture_gate,
        output wire interrupt_out,
        input wire clock_250,
        input wire S_AXI_ARESETN,
        wire timer_enable,
        wire [31:0] Cap_Timer_Out,
        wire [3:0] state,
	);

	input reg [32-1:0]	slv_reg0;
	input reg [32-1:0]	slv_reg1;
	input reg [32-1:0]	slv_reg2;
	input reg [32-1:0]	slv_reg3;
		parameter RESET = 3'b000,
        parameter COUNT = 3'b010,
        parameter WAIT = 3'b011,
        parameter IDLE = 3'b100

 

	// Add user logic here
    assign interrupt_out = slv_reg1[0];
    assign timer_enable = slv_reg1[1];
    
    // three flip-flops
    reg q1, q2, q3;
    always @(posedge clock_250, negedge S_AXI_ARESETN) begin
        if (S_AXI_ARESETN == 0) begin
            q1 <= 0;
            q2 <= 0;
            q3 <= 0;
        end else begin 
            q1 <= timer_enable;
            q2 <= q1;
            q3 <= q2;
            end
    end
    // state machine
	reg[3:0] current_state, next_state;
	reg[31:0] count = 0;
	always @ (negedge clock_250) begin
        if (S_AXI_ARESETN == 1'b0) begin
            current_state <= RESET;
        end 
        else begin
            current_state <= next_state;
        end
    end
    always @ (posedge clock_250) begin
        if (current_state == RESET) begin
            next_state <= IDLE;
        end
        if (current_state == IDLE) begin
            count=0;
            if (q3) begin
                next_state <= COUNT;
            end 
            else begin
                next_state <= IDLE;
            end
        end
        if (current_state == COUNT) begin
            if (capture_gate && !q3) begin
                next_state <= WAIT;
            end 
            else begin
                count <= count + 1;
                next_state <= COUNT;
            end
        end
        if (current_state == WAIT) begin
            if (q3) begin
                next_state <= WAIT;
            end 
            else begin
                next_state <= IDLE;
            end
        end        
    end
    assign Cap_Timer_Out = count;
    assign state = current_state; 
	// User logic ends

	endmodule
