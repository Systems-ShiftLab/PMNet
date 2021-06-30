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

PMNet implementation consists of 2 main parts: PMNet hardware and PMNet software library

## PMNet Hardware
There are 2 main parts of PMNet hardware implementation.
1. Datapath in FPGA (available at [PMNet_release/PMNet_Vivado/](PMNet_release/PMNet_Vivado/))
2. Packet processing (available at [PMNet_release/PMNet_P4/](PMNet_release/PMNet_P4/))

## PMNet Software
### Workloads
Workloads used for PMNet evaluation.    
Available Here: [PMNet_release/workloads/](PMNet_release/workloads/)
### PMNet library
PMNet library allows application to interface with PMNet devices.   
(Soon available)