#include "queue.h"
#include "lib.h"
#include "protocols.h"
#include <arpa/inet.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int rtable_len;
int arp_table_len;
struct route_table_entry * rtable;
struct arp_entry * arp_table;
int i = 0;

typedef struct {
	int len;
	char* content;
} queue_data;



uint32_t ip_address_to_uint32(char* ip_address) {
    struct in_addr addr;
 	int success = inet_pton(AF_INET, ip_address, &addr);
    return ntohl(addr.s_addr);
}



struct arp_entry *get_mac_entry(uint32_t ip_dest) {
	for (int i = 0; i < arp_table_len; i++) {
		if ((uint32_t)arp_table[i].ip == ip_dest)
			return &arp_table[i];
	}
	return NULL;
}

int binarySearch(int left, int right, uint32_t dest, struct route_table_entry *rtable) {
    if (left <= right) {
        if (rtable[(left + right)/2].prefix == (rtable[(left + right)/2].mask & dest))
            return (left + right)/2;
        else if (rtable[(left + right)/2].prefix >(rtable[(left + right)/2].mask & dest))
            binarySearch(left, (left + right)/2 - 1, dest, rtable);
        else
            binarySearch((left + right)/2 + 1, right, dest, rtable);
    }
    return -1;
}

struct route_table_entry *get_best_route(uint32_t dest, int rtable_len, struct route_table_entry *rtable) {
    struct route_table_entry *best_route = NULL;
	int idx = binarySearch(0, rtable_len, dest, rtable);
	for (int i = idx; i < rtable_len; i++){
		int x = dest & rtable[i].mask;
		if(x == rtable[i].prefix){
			if(best_route == NULL || (best_route->mask < rtable[i].mask))
				best_route = &rtable[i];
		}
	}
    return best_route;
}

int compare (const void *a, const void *b) {
	uint32_t prefix_a = ((struct route_table_entry *)a)->prefix;
	uint32_t prefix_b = ((struct route_table_entry *)b)->prefix;
	if(prefix_a == prefix_b)
		return (int)(((struct route_table_entry *)a)->mask - ((struct route_table_entry *)b)->mask);
	else
		return (prefix_a - prefix_b);
}



int main(int argc, char *argv[]){
	queue q = queue_create();
	char buf[MAX_PACKET_LEN];

	// Do not modify this line
	init(argc - 2, argv + 2);
	
	rtable = malloc(sizeof(struct route_table_entry) * 100000);
	DIE(rtable == NULL, "rtable memory");
	rtable_len = read_rtable(argv[1], rtable);



	arp_table = malloc(sizeof(struct arp_entry) * 100000);
	DIE(arp_table == NULL, "arptable memory");
	qsort(rtable, rtable_len, sizeof(struct route_table_entry), compare);			
	
	
	while (1) {

		int interface;
		size_t len;

		interface = recv_from_any_link(buf, &len);
		DIE(interface < 0, "recv_from_any_links");
		struct ether_header *eth_hdr = (struct ether_header *) buf;
		/* Note that packets received are in network order,
		any header field which has more than 1 byte will need to be conerted to
		host order. For example, ntohs(eth_hdr->ether_type). The oposite is needed when
		sending a packet on the link, */
		struct iphdr * ip_hdr = (struct iphdr*) (buf + sizeof(struct ether_header));
		struct arp_header* arp_hdr = (struct arp_header*) (buf + sizeof(struct ether_header));
		uint8_t* mac = malloc(sizeof(uint8_t) * 6);
		get_interface_mac(interface, mac);
		if(ntohs(eth_hdr->ether_type) == 0x0800) { 
			uint16_t check = ntohs(ip_hdr->check);
			ip_hdr->check = 0;
			uint16_t checksum_nou = checksum((uint16_t*)ip_hdr, sizeof(struct iphdr));
			ip_hdr->check = check;	
			int k = 0;
				if(ntohl(ip_address_to_uint32(get_interface_ip(interface))) == ip_hdr->daddr)
					k++;
				if(k != 0) {
					struct iphdr* iphdr_to_be_sent = (struct iphdr*)malloc(sizeof(struct iphdr));
					memcpy(iphdr_to_be_sent, ip_hdr, sizeof(struct iphdr));

					iphdr_to_be_sent->tot_len = ntohs(sizeof(struct iphdr) + sizeof(struct icmphdr));
					iphdr_to_be_sent->protocol = 1;
					iphdr_to_be_sent->saddr = ip_hdr->daddr;
					iphdr_to_be_sent->daddr = ip_hdr->saddr;

					struct ether_header* eth_to_be_sent = malloc(sizeof(struct ether_header));
					memcpy(eth_to_be_sent->ether_dhost, eth_hdr->ether_shost,6);
					memcpy(eth_to_be_sent->ether_shost, eth_hdr->ether_dhost,6);
					eth_to_be_sent->ether_type = eth_hdr->ether_type;

					struct icmphdr* icmphdr_to_be_sent = malloc(sizeof(struct icmphdr));
					icmphdr_to_be_sent->type = 0;
					icmphdr_to_be_sent->code = 0;
					icmphdr_to_be_sent->un.frag.mtu = 0;
					icmphdr_to_be_sent->un.frag.__unused = 0;
					icmphdr_to_be_sent->un.echo.id = 0;
					icmphdr_to_be_sent->un.echo.sequence = 0;
					icmphdr_to_be_sent->un.gateway = 0;
					icmphdr_to_be_sent->checksum = 0;
					icmphdr_to_be_sent->checksum = ntohl(checksum((uint16_t*)(icmphdr_to_be_sent),sizeof(struct icmphdr)));

					char* to_be_sent = malloc(2*sizeof(struct iphdr) + sizeof(struct ether_header) + sizeof(struct icmphdr));
					memcpy(to_be_sent, eth_to_be_sent, sizeof(struct ether_header));
					memcpy(to_be_sent + sizeof(struct ether_header), iphdr_to_be_sent, sizeof(struct iphdr));
					memcpy(to_be_sent + sizeof(struct iphdr) + sizeof(struct ether_header), icmphdr_to_be_sent, sizeof(struct icmphdr));
					memcpy(to_be_sent + sizeof(struct iphdr) + sizeof(struct ether_header) + sizeof(struct icmphdr), ip_hdr, sizeof(struct iphdr));

					send_to_link(interface, to_be_sent, 2*sizeof(struct iphdr) + sizeof(struct ether_header) + sizeof(struct icmphdr));
					continue;
				} else {
					if(check == checksum_nou) {
						ip_hdr->ttl--;
						if(ip_hdr->ttl <= 1){
							struct iphdr* iphdr_to_be_sent = (struct iphdr*)malloc(sizeof(struct iphdr));
							memcpy(iphdr_to_be_sent, ip_hdr, sizeof(struct iphdr));

							iphdr_to_be_sent->tot_len = ntohs(sizeof(struct iphdr) + sizeof(struct icmphdr));
							iphdr_to_be_sent->protocol = 1;
							iphdr_to_be_sent->saddr = ip_hdr->daddr;
							iphdr_to_be_sent->daddr = ip_hdr->saddr;

							struct ether_header* eth_to_be_sent = malloc(sizeof(struct ether_header));
							memcpy(eth_to_be_sent->ether_dhost, eth_hdr->ether_shost,6);
							memcpy(eth_to_be_sent->ether_shost, eth_hdr->ether_dhost,6);
							eth_to_be_sent->ether_type = eth_hdr->ether_type;

							struct icmphdr* icmphdr_to_be_sent = malloc(sizeof(struct icmphdr));
							icmphdr_to_be_sent->type = 11;
							icmphdr_to_be_sent->code = 0;
							icmphdr_to_be_sent->un.frag.mtu = 0;
							icmphdr_to_be_sent->un.frag.__unused = 0;
							icmphdr_to_be_sent->un.echo.id = 0;
							icmphdr_to_be_sent->un.echo.sequence = 0;
							icmphdr_to_be_sent->un.gateway = 0;
							icmphdr_to_be_sent->checksum = 0;
							icmphdr_to_be_sent->checksum = ntohl(checksum((uint16_t*)(icmphdr_to_be_sent),sizeof(struct icmphdr)));

							char* to_be_sent = malloc(2*sizeof(struct iphdr) + sizeof(struct ether_header) + sizeof(struct icmphdr));
							memcpy(to_be_sent, eth_to_be_sent, sizeof(struct ether_header));
							memcpy(to_be_sent + sizeof(struct ether_header), iphdr_to_be_sent, sizeof(struct iphdr));
							memcpy(to_be_sent + sizeof(struct iphdr) + sizeof(struct ether_header), icmphdr_to_be_sent, sizeof(struct icmphdr));
							memcpy(to_be_sent + sizeof(struct iphdr) + sizeof(struct ether_header) + sizeof(struct icmphdr), ip_hdr, sizeof(struct iphdr));

							send_to_link(interface, to_be_sent, 2*sizeof(struct iphdr) + sizeof(struct ether_header) + sizeof(struct icmphdr));
							continue;
					}
							
					 struct route_table_entry *ruta__buna = get_best_route(ip_hdr->daddr, rtable_len ,rtable);
					

					if(ruta__buna == NULL) {
						struct iphdr* iphdr_to_be_sent = (struct iphdr*)malloc(sizeof(struct iphdr));
						memcpy(iphdr_to_be_sent, ip_hdr, sizeof(struct iphdr));


						iphdr_to_be_sent->tot_len = ntohs(sizeof(struct iphdr) + sizeof(struct icmphdr));
						iphdr_to_be_sent->protocol = 1;
						iphdr_to_be_sent->saddr = ip_hdr->daddr;
						iphdr_to_be_sent->daddr = ip_hdr->saddr;

						struct ether_header* eth_to_be_sent = malloc(sizeof(struct ether_header));
						memcpy(eth_to_be_sent->ether_dhost, eth_hdr->ether_shost,6);
						memcpy(eth_to_be_sent->ether_shost, eth_hdr->ether_dhost,6);
						eth_to_be_sent->ether_type = eth_hdr->ether_type;

						struct icmphdr* icmphdr_to_be_sent = malloc(sizeof(struct icmphdr));
						icmphdr_to_be_sent->type = 3;
						icmphdr_to_be_sent->code = 0;
						icmphdr_to_be_sent->un.frag.mtu = 0;
						icmphdr_to_be_sent->un.frag.__unused = 0;
						icmphdr_to_be_sent->un.echo.id = 0;
						icmphdr_to_be_sent->un.echo.sequence = 0;
						icmphdr_to_be_sent->un.gateway = 0;
						icmphdr_to_be_sent->checksum = 0;
						icmphdr_to_be_sent->checksum = ntohl(checksum((uint16_t*)(icmphdr_to_be_sent),sizeof(struct icmphdr)));

						char* to_be_sent = malloc(2*sizeof(struct iphdr) + sizeof(struct ether_header) + sizeof(struct icmphdr));
						memcpy(to_be_sent, eth_to_be_sent, sizeof(struct ether_header));
						memcpy(to_be_sent + sizeof(struct ether_header), iphdr_to_be_sent, sizeof(struct iphdr));
						memcpy(to_be_sent + sizeof(struct iphdr) + sizeof(struct ether_header), icmphdr_to_be_sent, sizeof(struct icmphdr));
						memcpy(to_be_sent + sizeof(struct iphdr) + sizeof(struct ether_header) + sizeof(struct icmphdr), ip_hdr, sizeof(struct iphdr));

						send_to_link(interface, to_be_sent, sizeof(struct iphdr) + sizeof(struct ether_header) + sizeof(struct icmphdr));
						continue;
					}
					
					ip_hdr->check = 0;
					ip_hdr->check = ntohs(checksum((uint16_t*)ip_hdr, sizeof(struct iphdr)));


					
					struct arp_entry* mac_entry = get_mac_entry(ruta__buna->next_hop);
					

					
					if(mac_entry == NULL) {
						char* buffer = malloc(MAX_PACKET_LEN);
						memcpy(buffer, buf, len);
						queue_data* queue_data = malloc(sizeof(queue_data));
						queue_data->len = len;
						queue_data->content = buffer;
						queue_enq(q, queue_data);
						uint8_t sha[6];
						get_interface_mac(ruta__buna->interface, sha);

						struct ether_header *eth_hdr_arp = malloc(sizeof(struct ether_header));
						for(int i = 0; i < 6; i++) {
							eth_hdr_arp->ether_dhost[i] = 0xFF;
						}
						memcpy(eth_hdr_arp->ether_shost, sha, 6);

						eth_hdr_arp->ether_type = htons(0x0806);

						struct arp_header *arp_hdr_arp = malloc(sizeof(struct arp_header));
						arp_hdr_arp->ptype = htons(0x0800);
						arp_hdr_arp->plen = 4;
						arp_hdr_arp->hlen = 6;
						arp_hdr_arp->htype = htons(1);
						arp_hdr_arp->op =htons(1);
						
						for(int i = 0; i < 6; i++){
							arp_hdr_arp->tha[i] = 0x00;
						}
						memcpy(arp_hdr_arp->sha, sha, 6);
						
						arp_hdr_arp->spa = htonl(ip_address_to_uint32(get_interface_ip(ruta__buna->interface)));
						arp_hdr_arp->tpa = ruta__buna->next_hop;
						char * new = malloc(sizeof(struct ether_header) + sizeof(struct arp_header));
						memcpy(new, eth_hdr_arp, sizeof(struct ether_header));
						memcpy(new +sizeof(struct ether_header), arp_hdr_arp, sizeof(struct arp_header));
						send_to_link(ruta__buna->interface, new, sizeof(struct ether_header) + sizeof(struct arp_header));
						continue;
					}	
					
					memcpy(eth_hdr->ether_dhost, mac_entry->mac, 6);
					memcpy(eth_hdr->ether_shost, mac, 6);
					send_to_link(ruta__buna->interface, buf, len);
					continue;
				}
			}
		} else if(ntohs(eth_hdr->ether_type) == 0x0806) { 
			if(ntohs(arp_hdr->op) == 1){ 
				if(ntohl(arp_hdr->tpa) == ip_address_to_uint32(get_interface_ip(interface))) {
				
					uint32_t arp_ip_source = arp_hdr->spa;
					uint32_t arp_ip_target = arp_hdr->tpa;
					arp_hdr->spa = arp_ip_target;
					arp_hdr->tpa = arp_ip_source;
					uint8_t * dhost = malloc(6 * sizeof(uint8_t));
					uint8_t * shost = mac;

					memcpy(dhost, eth_hdr->ether_shost, 6);

					memcpy(arp_hdr->sha, shost, 6);

					memcpy(arp_hdr->tha, dhost, 6);
					memcpy(eth_hdr->ether_shost, shost, 6);
					memcpy(eth_hdr->ether_dhost, dhost, 6);

					arp_hdr->op = htons(2);
					
					send_to_link(interface, buf, sizeof(struct ether_header) + sizeof(struct arp_header));
					continue;
				}
			} else if(ntohs(arp_hdr->op) == 2) {
				if(queue_empty(q) == 0){
					queue_data* deq = queue_deq(q);
					struct ether_header* eth = (struct ether_header*) deq->content;
					struct ether_header* eth_buf = (struct ether_header*) buf;
					memcpy(eth->ether_dhost, eth_buf->ether_shost, 6);
					memcpy(eth_hdr->ether_shost, eth_buf->ether_dhost, 6);
					send_to_link(interface, deq->content, deq->len);
					arp_table[arp_table_len].ip = arp_hdr->spa;
					memcpy(&(arp_table[arp_table_len].mac), &(arp_hdr->sha), 6);
					arp_table_len++;
				}
	 		}
			continue;
		}
	}
}

