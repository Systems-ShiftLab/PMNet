//----------------------------------------------------------------------------
//   This file is owned and controlled by Xilinx and must be used solely    //
//   for design, simulation, implementation and creation of design files    //
//   limited to Xilinx devices or technologies. Use with non-Xilinx         //
//   devices or technologies is expressly prohibited and immediately        //
//   terminates your license.                                               //
//                                                                          //
//   XILINX IS PROVIDING THIS DESIGN, CODE, OR INFORMATION "AS IS" SOLELY   //
//   FOR USE IN DEVELOPING PROGRAMS AND SOLUTIONS FOR XILINX DEVICES.  BY   //
//   PROVIDING THIS DESIGN, CODE, OR INFORMATION AS ONE POSSIBLE            //
//   IMPLEMENTATION OF THIS FEATURE, APPLICATION OR STANDARD, XILINX IS     //
//   MAKING NO REPRESENTATION THAT THIS IMPLEMENTATION IS FREE FROM ANY     //
//   CLAIMS OF INFRINGEMENT, AND YOU ARE RESPONSIBLE FOR OBTAINING ANY      //
//   RIGHTS YOU MAY REQUIRE FOR YOUR IMPLEMENTATION.  XILINX EXPRESSLY      //
//   DISCLAIMS ANY WARRANTY WHATSOEVER WITH RESPECT TO THE ADEQUACY OF THE  //
//   IMPLEMENTATION, INCLUDING BUT NOT LIMITED TO ANY WARRANTIES OR         //
//   REPRESENTATIONS THAT THIS IMPLEMENTATION IS FREE FROM CLAIMS OF        //
//   INFRINGEMENT, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A  //
//   PARTICULAR PURPOSE.                                                    //
//                                                                          //
//   Xilinx products are not intended for use in life support appliances,   //
//   devices, or systems.  Use in such applications are expressly           //
//   prohibited.                                                            //
//                                                                          //
//   (c) Copyright 1995-2017 Xilinx, Inc.                                   //
//   All rights reserved.                                                   //
//----------------------------------------------------------------------------
#ifndef _XILINX_P4_
#define _XILINX_P4_

#include "core.p4"
#include "xilinx_core.p4"

/* struct switch_metadata_t - sample control struct
 *
 *  a user can choose to not use this struct in their switch and substitute
 *  their own struct. this declaration is provided as a default
 */
// typedef bit<4> switch_port_t;
// struct switch_metadata_t {
//     switch_port_t       ingress_port;
//     switch_port_t       egress_port;
// }

typedef bit<32> hashedAddress_t;
typedef bit<8>  PMSwitchOPS_t;
typedef bit<8>  ackCount_t;
struct PMswitch_metadata_t {
    hashedAddress_t     hashedAddress;
    PMSwitchOPS_t       PMSwitchOPS;
    ackCount_t          ackCount;
    //bit<1>              ackMode;
}


/* parser XilinxParser - user defined parser
 *  @param packet:      incomming packet
 *  @param local:       data passed beween components
 */
parser XilinxParser<L>(
    packet_in packet, out L local);

/* control XilinxPipeline - user defined Match+Action pipeline
 *  @param local:       data passed between components
 *  @param metadata:    I/O switch metadata
 */
control XilinxPipeline<L,C>(
    inout L local, inout C metadata);

/* control XilinxDeparser - user defined deparser
 *  @param local:       data passed between components
 *  @param packet:      outgoing packet
 */
control XilinxDeparser<L>(
    in L local, packet_out packet);

/* packet XilinxSwitch - toplevel description of the switch
 *
 *          XilinxParser        XilinxPipeline      XilinxDeparser
 *      --> packet                                  packet --------->
 *      ----------------------> metadata --------------------------->
 *          local ------------> local ------------> local
 */
package XilinxSwitch<L,C>(
    XilinxParser<L> user_defined_parser,
    XilinxPipeline<L,C> user_defined_pipeline,
    XilinxDeparser<L> user_defined_deparser);

/* control XilinxStreamDeparser - user defined deparser
 *  @param local:       data passed between components
 *  @param packet:      outgoing packet
 */
parser XilinxStreamDeparser<L>(
    in L local, packet_mod packet);

/* packet XilinxSwitch - toplevel description of the switch
 *
 *          XilinxStreamParser  XilinxPipeline      XilinxStreamDeparser
 *      --> packet                                  packet --------->
 *      ----------------------> metadata --------------------------->
 *          local ------------> local ------------> local
 */
package XilinxStreamSwitch<L,C>(
    XilinxParser<L>         user_defined_parser,
    XilinxPipeline<L,C>     user_defined_pipeline,
    XilinxStreamDeparser<L> user_defined_deparser);

#endif  /* _XILINX_P4_ */
