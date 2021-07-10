# PMNet

PMNet is a system that accelerates update requests by logging update requests in network device.

If you find PMNet useful in your research, please cite:

> Korakit Seemakhupt, Sihang Liu, Yasas Senevirathne, Muhammad Shahbaz and Samira Khan  
> [PMNet: In-Network Data Persistence](https://www.cs.virginia.edu/~smk9u/PMNet_ISCA2021.pdf) 
> The International Symposium on Computer Architecture (ISCA), 2021

<details><summary><i>BibTex</i></summary>
<p>

```
@inproceedings{seemakhupt2021pmnet,
  title={PMNet: In-Network Data Persistence},
  author={Seemakhupt, Korakit and Liu, Sihang and Senevirathne, Yasas and Shahbaz, Muhammad and Khan, Samira},
  booktitle={2021 ACM/IEEE 48th Annual International Symposium on Computer Architecture (ISCA)},
  year={2021}
}
```

</p>
</details>

## Testing PMNet
### Tested Environment
1. Xilinx VCU118 FPGA
2. Server Machine: Intel Cascade Lake, 128GiB DRAM, 256GiB DCPMM, Mellanox CX353A NIC, Ubuntu 20.04
3. Client Machine: Intel Haswell, 128GiB DRAM, Mellanox CX353A NIC, Ubuntu 20.04
4. 10 Gbit/s ethernet switches as ToR switches

### Tested Topology
Servers <---> ToR switch - FPGA <---> ToR switch <---> Clients

### Running the test
1. Build the hardware and program the bitstream ([Hardware](PMNet_release/PMNet_Vivado/))
2. Build and run the workloads ([Workloads](PMNet_release/workloads/))


PMNet implementation consists of 2 main parts: PMNet hardware and PMNet software

## PMNet Hardware
There are 2 main parts of PMNet hardware implementation.
1. Packet processing (available at [PMNet_release/PMNet_P4/](PMNet_release/PMNet_P4/))
2. Datapath in FPGA (available at [PMNet_release/PMNet_Vivado/](PMNet_release/PMNet_Vivado/))

## PMNet Software
### Workloads
Workloads used for PMNet evaluation.    
Available Here: [PMNet_release/workloads/](PMNet_release/workloads/)

