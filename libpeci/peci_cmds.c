/*
// Copyright (c) 2019 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/
#include <inttypes.h>
#include <peci.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#ifndef ABS
#define ABS(_v_) (((_v_) > 0) ? (_v_) : -(_v_))
#endif

extern EPECIStatus peci_GetDIB(uint8_t target, uint64_t* dib);

void Usage(char* progname)
{
    printf("Usage :\n");
    printf("\t%s [-a <addr>] [-s <size>] <command> [parameters]\n", progname);
    printf("\t\t-a  : Address of the target in decimal. Default is 48.\n");
    printf("\t\t-s  : Size of data to read or write in bytes. Default is 4.\n");
    printf("\t\t-b  : Display completion code.\n");
    printf("\t\t-n  : Ping the target.\n");
    printf("\t\t-t  : Get the temperature.\n");
    printf("\t\t-d  : Get the DIB.\n");
    printf(
        "\t\t-p  : PCI Read for specific hex address <Bus Dev Func [Reg]>.\n");
    printf("\t\t-pw : PCI Write for specific hex address <Bus Dev Func [Reg] "
           "Data>.\n");
    printf("\t\t-c  : Read Package Config <Index Parameter>.\n");
    printf("\t\t-cw : Write Package Config <Index Parameter Data>.\n");
    printf("\t\t-m  : MSR Read <Thread Address>.\n");
    printf("\t\t-l  : Local PCI Read for specific hex address <Bus Dev Func "
           "[Reg]>.\n");
    printf("\t\t-lw : Local PCI Write for specific hex address <Bus Dev Func "
           "[Reg] Data>.\n");
    printf("\n");
}

int main(int argc, char* argv[])
{
    int c;
    EPECIStatus ret;
    int cnt = 0;
    uint8_t u8Cmd = PECI_CMD_MAX;
    uint8_t address = 0x30; // use default address of 48d
    uint8_t u8Size = 4;     // default to a DWORD
    uint32_t u32PciReadVal = 0;
    uint8_t u8PciBus = 0;
    uint8_t u8PciDev = 0;
    uint8_t u8PciFunc = 0;
    uint16_t u16PciReg = 0;
    uint32_t u32PciWriteVal = 0;
    uint8_t u8PkgIndex = 0;
    uint16_t u16PkgParam = 0;
    uint32_t u32PkgValue = 0;
    uint8_t u8MsrThread = 0;
    uint16_t u16MsrAddr = 0;
    uint64_t u64MsrVal = 0;
    short temperature;
    uint64_t dib;
    uint8_t u8Index = 0;
    uint8_t cc = 0;
    bool showCc = false;

    //
    // Parse arguments.
    //
    while (-1 != (c = getopt(argc, argv, "a:s:nbtdp::c::m::l::")))
    {
        switch (c)
        {
            case 'a':
                if (optarg != NULL)
                    address = (unsigned char)atoi(optarg);
                break;

            case 's':
                if (optarg != NULL)
                    u8Size = (unsigned char)atoi(optarg);
                break;

            case 'b':
                showCc = true;
                break;

            case 'n':
                cnt++;
                u8Cmd = PECI_CMD_PING;
                break;

            case 't':
                cnt++;
                u8Cmd = PECI_CMD_GET_TEMP;
                break;

            case 'd':
                cnt++;
                u8Cmd = PECI_CMD_GET_DIB;
                break;

            case 'p':
                cnt++;
                u8Cmd = PECI_CMD_RD_PCI_CFG;
                if (optarg != NULL && optarg[0] == 'w')
                    u8Cmd = PECI_CMD_WR_PCI_CFG;
                break;

            case 'c':
                cnt++;
                u8Cmd = PECI_CMD_RD_PKG_CFG;
                if (optarg != NULL && optarg[0] == 'w')
                    u8Cmd = PECI_CMD_WR_PKG_CFG;
                break;

            case 'm':
                cnt++;
                u8Cmd = PECI_CMD_RD_IA_MSR;
                break;

            case 'l':
                cnt++;
                u8Cmd = PECI_CMD_RD_PCI_CFG_LOCAL;
                if (optarg != NULL && optarg[0] == 'w')
                    u8Cmd = PECI_CMD_WR_PCI_CFG_LOCAL;
                break;

            default:
                printf("ERROR: Unrecognized option \"-%c\".\n", optopt);
                goto ErrorExit;
                break;
        }
    }

    if (1 != cnt)
    {
        printf("ERROR: Invalid options.\n");
        goto ErrorExit;
    }

    //
    // Execute the command
    //
    printf("PECI target[%u]: ", (int)address);
    switch (u8Cmd)
    {
        case PECI_CMD_PING:
            printf("Pinging ... ");
            (0 == peci_Ping(address)) ? printf("Succeeded.\n")
                                      : printf("Failed.\n");
            break;

        case PECI_CMD_GET_TEMP:
            ret = peci_GetTemp(address, &temperature);
            if (0 != ret)
            {
                printf("ERROR: Retrieving temperature failed.\n");
                break;
            }
            printf("Temperature is %04xh (%c%d.%02dC).\n",
                   (int)(unsigned int)(unsigned short)temperature,
                   (0 > temperature) ? '-' : '+',
                   (int)((unsigned int)ABS(temperature) / 64),
                   (int)(((unsigned int)ABS(temperature) % 64) * 100) / 64);
            break;

        case PECI_CMD_GET_DIB:
            ret = peci_GetDIB(address, &dib);
            if (0 != ret)
            {
                printf("ERROR: Retrieving DIB failed.\n");
                break;
            }
            printf("GetDIB Returned: 0x%" PRIx64 "\n", dib);
            break;

        case PECI_CMD_RD_PCI_CFG:
            u8Index = argc;
            switch (argc - optind)
            {
                case 4:
                    u16PciReg = strtoul(argv[--u8Index], NULL, 16);
                case 3:
                    u8PciFunc = strtoul(argv[--u8Index], NULL, 16);
                case 2:
                    u8PciDev = strtoul(argv[--u8Index], NULL, 16);
                case 1:
                    u8PciBus = strtoul(argv[--u8Index], NULL, 16);
                    break;
                default:
                    printf("ERROR: Unsupported arguments for PCI Write\n");
                    goto ErrorExit;
                    break;
            }
            ret = peci_RdPCIConfig(address, u8PciBus, u8PciDev, u8PciFunc,
                                   u16PciReg, (uint8_t*)&u32PciReadVal, &cc);
            if (showCc)
                printf("PCI Read cc:0x%x\n", cc);
            if (0 != ret)
            {
                printf("ERROR: PCI Read failed or is not supported.\n");
                break;
            }
            printf("PCI Read of %02x:%02x:%02x Reg %02x: 0x%0*x\n", u8PciBus,
                   u8PciDev, u8PciFunc, u16PciReg, u8Size * 2, u32PciReadVal);
            break;

        case PECI_CMD_RD_PKG_CFG:
            u8Index = argc;
            switch (argc - optind)
            {
                case 2:
                    u16PkgParam = strtoul(argv[--u8Index], NULL, 16);
                    u8PkgIndex = strtoul(argv[--u8Index], NULL, 16);
                    break;
                default:
                    printf("ERROR: Unsupported arguments for Pkg Read\n");
                    goto ErrorExit;
                    break;
            }
            ret = peci_RdPkgConfig(address, u8PkgIndex, u16PkgParam, u8Size,
                                   (uint8_t*)&u32PkgValue, &cc);
            if (showCc)
                printf("Pkg Read cc:0x%x\n", cc);
            if (0 != ret)
            {
                printf("ERROR: Read Package failed or is not supported.\n");
                break;
            }
            printf("Pkg Read of Index %02x Param %04x: 0x%0*x\n", u8PkgIndex,
                   u16PkgParam, u8Size * 2, u32PkgValue);
            break;

        case PECI_CMD_WR_PKG_CFG:
            u8Index = argc;
            switch (argc - optind)
            {
                case 3:
                    u32PkgValue = strtoul(argv[--u8Index], NULL, 16);
                    u16PkgParam = strtoul(argv[--u8Index], NULL, 16);
                    u8PkgIndex = strtoul(argv[--u8Index], NULL, 16);
                    break;
                default:
                    printf("ERROR: Unsupported arguments for Pkg Write\n");
                    goto ErrorExit;
                    break;
            }
            printf("Pkg Write of Index %02x Param %04x: 0x%0*x\n", u8PkgIndex,
                   u16PkgParam, u8Size * 2, u32PkgValue);
            ret = peci_WrPkgConfig(address, u8PkgIndex, u16PkgParam,
                                   u32PkgValue, u8Size, &cc);
            if (showCc)
                printf("Pkg Write cc:0x%x\n", cc);
            (0 == ret) ? printf("Succeeded.\n")
                       : printf("Failed or not supported.\n");
            break;

        case PECI_CMD_RD_IA_MSR:
            u8Index = argc;
            switch (argc - optind)
            {
                case 2:
                    u16MsrAddr = strtoul(argv[--u8Index], NULL, 16);
                    u8MsrThread = strtoul(argv[--u8Index], NULL, 16);
                    break;
                default:
                    printf("ERROR: Unsupported arguments for MSR Read\n");
                    goto ErrorExit;
                    break;
            }
            ret =
                peci_RdIAMSR(address, u8MsrThread, u16MsrAddr, &u64MsrVal, &cc);
            if (showCc)
                printf("MSR Read cc:0x%x\n", cc);
            if (0 != ret)
            {
                printf("ERROR: Read MSR failed or is not supported. %x\n", ret);
                break;
            }
            printf("MSR Read of Thread %02x MSR %04x: 0x%0*" PRIx64 "\n",
                   u8MsrThread, u16MsrAddr, u8Size * 2, u64MsrVal);
            break;

        case PECI_CMD_RD_PCI_CFG_LOCAL:
            u8Index = argc;
            switch (argc - optind)
            {
                case 4:
                    u16PciReg = strtoul(argv[--u8Index], NULL, 16);
                case 3:
                    u8PciFunc = strtoul(argv[--u8Index], NULL, 16);
                case 2:
                    u8PciDev = strtoul(argv[--u8Index], NULL, 16);
                case 1:
                    u8PciBus = strtoul(argv[--u8Index], NULL, 16);
                    break;
                default:
                    printf(
                        "ERROR: Unsupported arguments for Local PCI Write\n");
                    goto ErrorExit;
                    break;
            }
            ret = peci_RdPCIConfigLocal(address, u8PciBus, u8PciDev, u8PciFunc,
                                        u16PciReg, u8Size,
                                        (uint8_t*)&u32PciReadVal, &cc);
            if (showCc)
                printf("Local PCI Read cc:0x%x\n", cc);
            if (0 != ret)
            {
                printf("ERROR: Local PCI Read failed or is not supported.\n");
                break;
            }
            printf("Local PCI Read of %02x:%02x:%02x Reg %02x: 0x%0*x\n",
                   u8PciBus, u8PciDev, u8PciFunc, u16PciReg, u8Size * 2,
                   u32PciReadVal);
            break;

        case PECI_CMD_WR_PCI_CFG_LOCAL:
            u8Index = argc;
            u32PciWriteVal = strtoul(argv[--u8Index], NULL, 16);
            switch (argc - optind)
            {
                case 5:
                    u16PciReg = strtoul(argv[--u8Index], NULL, 16);
                case 4:
                    u8PciFunc = strtoul(argv[--u8Index], NULL, 16);
                case 3:
                    u8PciDev = strtoul(argv[--u8Index], NULL, 16);
                case 2:
                    u8PciBus = strtoul(argv[--u8Index], NULL, 16);
                    break;
                default:
                    printf(
                        "ERROR: Unsupported arguments for Local PCI Write\n");
                    goto ErrorExit;
                    break;
            }
            printf("Local PCI Write of %02x:%02x:%02x Reg %02x: 0x%0*x\n",
                   u8PciBus, u8PciDev, u8PciFunc, u16PciReg, u8Size * 2,
                   u32PciWriteVal);
            ret = peci_WrPCIConfigLocal(address, u8PciBus, u8PciDev, u8PciFunc,
                                        u16PciReg, u8Size, u32PciWriteVal, &cc);
            if (showCc)
                printf("Local PCI Write cc:0x%x\n", cc);
            (0 == ret) ? printf("Succeeded.\n")
                       : printf("Failed or not supported.\n");
            break;

        default:
            printf("ERROR: Unrecognized command\n");
            goto ErrorExit;
            break;
    }
    return 0;

ErrorExit:
    Usage(argv[0]);
    return 1;
}
