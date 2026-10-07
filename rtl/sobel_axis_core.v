`timescale 1 ps / 1 ps

module sobel_axis_core #
(
    parameter integer DATA_WIDTH  = 32,
    parameter integer IMAGE_WIDTH = 640
)
(
    input  wire                      aclk,
    input  wire                      aresetn,

    input  wire [DATA_WIDTH-1:0]     s_axis_tdata,
    input  wire                      s_axis_tvalid,
    output wire                      s_axis_tready,
    input  wire                      s_axis_tlast,

    output wire [DATA_WIDTH-1:0]     m_axis_tdata,
    output wire                      m_axis_tvalid,
    input  wire                      m_axis_tready,
    output wire                      m_axis_tlast
);

    reg [7:0] linebuf0 [0:IMAGE_WIDTH-1];
    reg [7:0] linebuf1 [0:IMAGE_WIDTH-1];

    reg [7:0] w00, w01, w02;
    reg [7:0] w10, w11, w12;
    reg [7:0] w20, w21, w22;

    reg [15:0] x_cnt;
    reg [15:0] y_cnt;

    reg [DATA_WIDTH-1:0] m_axis_tdata_reg;
    reg                  m_axis_tvalid_reg;
    reg                  m_axis_tlast_reg;

    integer i;

    assign s_axis_tready = (~m_axis_tvalid_reg) || m_axis_tready;

    assign m_axis_tdata  = m_axis_tdata_reg;
    assign m_axis_tvalid = m_axis_tvalid_reg;
    assign m_axis_tlast  = m_axis_tlast_reg;

    wire [7:0] bot_new_w = s_axis_tdata[7:0];
    wire [7:0] mid_new_w = linebuf0[x_cnt];
    wire [7:0] top_new_w = linebuf1[x_cnt];

    wire signed [10:0] gx_w;
    wire signed [10:0] gy_w;
    wire [10:0] abs_gx_w;
    wire [10:0] abs_gy_w;
    wire [11:0] mag_w;
    wire [7:0]  out_pixel_w;

    assign gx_w =
        $signed({1'b0, top_new_w}) + ($signed({1'b0, mid_new_w}) <<< 1) + $signed({1'b0, bot_new_w})
      - $signed({1'b0, w01})       - ($signed({1'b0, w11}) <<< 1)       - $signed({1'b0, w21});

    assign gy_w =
        $signed({1'b0, w01})       + ($signed({1'b0, w02}) <<< 1)       + $signed({1'b0, top_new_w})
      - $signed({1'b0, w21})       - ($signed({1'b0, w22}) <<< 1)       - $signed({1'b0, bot_new_w});

    assign abs_gx_w = gx_w[10] ? -gx_w : gx_w;
    assign abs_gy_w = gy_w[10] ? -gy_w : gy_w;
    assign mag_w    = abs_gx_w + abs_gy_w;

    assign out_pixel_w = (mag_w > 12'd255) ? 8'hFF : mag_w[7:0];

    always @(posedge aclk) begin
        if (!aresetn) begin
            x_cnt <= 16'd0;
            y_cnt <= 16'd0;

            w00 <= 8'd0; w01 <= 8'd0; w02 <= 8'd0;
            w10 <= 8'd0; w11 <= 8'd0; w12 <= 8'd0;
            w20 <= 8'd0; w21 <= 8'd0; w22 <= 8'd0;

            m_axis_tdata_reg  <= {DATA_WIDTH{1'b0}};
            m_axis_tvalid_reg <= 1'b0;
            m_axis_tlast_reg  <= 1'b0;

            for (i = 0; i < IMAGE_WIDTH; i = i + 1) begin
                linebuf0[i] <= 8'd0;
                linebuf1[i] <= 8'd0;
            end
        end
        else begin
            if (s_axis_tready) begin
                m_axis_tvalid_reg <= s_axis_tvalid;

                if (s_axis_tvalid) begin
                    linebuf1[x_cnt] <= linebuf0[x_cnt];
                    linebuf0[x_cnt] <= bot_new_w;

                    w00 <= w01; w01 <= w02; w02 <= top_new_w;
                    w10 <= w11; w11 <= w12; w12 <= mid_new_w;
                    w20 <= w21; w21 <= w22; w22 <= bot_new_w;

                    if ((x_cnt < 2) || (y_cnt < 2))
                        m_axis_tdata_reg <= 32'd0;
                    else
                        m_axis_tdata_reg <= {24'd0, out_pixel_w};

                    m_axis_tlast_reg <= s_axis_tlast;

                    if (x_cnt == IMAGE_WIDTH - 1) begin
                        x_cnt <= 16'd0;
                        y_cnt <= y_cnt + 16'd1;
                    end
                    else begin
                        x_cnt <= x_cnt + 16'd1;
                    end
                end
                else begin
                    m_axis_tdata_reg <= {DATA_WIDTH{1'b0}};
                    m_axis_tlast_reg <= 1'b0;
                end
            end
        end
    end

endmodule
