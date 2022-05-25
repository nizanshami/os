#include "os.h"


/*  create\destroy virtual memory mapping in page table
    pt - physical page number of the root of pt
    vpn - virtual page number
    ppn - ppn == NO_MAPPING ? destroy vpn mapping :  map the vpn to ppn
     */
void page_table_update(uint64_t pt, uint64_t vpn, uint64_t ppn){
    uint64_t *cur_node = (uint64_t *) phys_to_virt(pt << 12); /* set offset to zero to start in the begining of the page */
    int cur_pte;
    int i;
    
    for (i = 0;i < 4; i++){
        cur_pte = (vpn >> (36 - 9 * i)) & 0x1ff; /* the 9 bits to get the nect node */
        if((cur_node[cur_pte] & 1) == 0){/*chack if the next node don't exist*/
            if(ppn == NO_MAPPING){/* if this ppn shouldn't have a mapping we just return*/
                return;
            }else{/*creat next node*/ 
                cur_node[cur_pte] = (alloc_page_frame() << 12) + 1;/* creat the node in the next level, point to the top of the node and make the pte valid*/
            }

        }
        cur_node = phys_to_virt(cur_node[cur_pte] -1);// move pointer to the next node

    }
    cur_pte = (vpn & 0x1ff);// get last entry
    if(ppn == NO_MAPPING){
        cur_node[cur_pte] = 0;//destroy mapping
        return;
    }else{
        cur_node[cur_pte] = (ppn << 12) + 1;//create mapping
        return;
    }

}

/*  return the physical page number that vpn is map to or NO_MAPPING
    pt - physical page number of the root of pt
    vpn - virtual page number
    */
uint64_t page_table_query(uint64_t pt, uint64_t vpn){
    uint64_t *cur_node = (uint64_t *)phys_to_virt(pt << 12); /* set offset to zero to start in the begining of the page */
    int cur_pte;
    int i;
    
    for (i = 0;i < 4; i++){
        cur_pte = (vpn >> (36 - 9 * i)) & 0x1ff; /* the 9 bits to get the nect node */
        if((cur_node[cur_pte] & 1) == 0){/*chack if the next node don't exist*/
            return NO_MAPPING;
        }
        cur_node = phys_to_virt(cur_node[cur_pte] -1);// move pointer to the next node
    }
    cur_pte = (vpn & 0x1ff);// get last entry
    if((cur_node[cur_pte] & 1) == 0){/*chack if the next node don't exist*/
            return NO_MAPPING;
    }else{
        return cur_node[cur_pte] >> 12;
    }
}


