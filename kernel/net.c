#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "net.h"

// xv6's ethernet and IP addresses
static uint8 local_mac[ETHADDR_LEN] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
static uint32 local_ip = MAKE_IP_ADDR(10, 0, 2, 15);

// qemu host's ethernet address.
static uint8 host_mac[ETHADDR_LEN] = { 0x52, 0x55, 0x0a, 0x00, 0x02, 0x02 };

static struct spinlock netlock;

#define RECV_QUEUE_SIZE 16
#define RECV_PORT_MAX 1024

static struct recv_port{
  uint16 port;
  int queue_head, queue_tail; // head out, tail in
  int queue_size;
  char *bufs[RECV_QUEUE_SIZE];
  int nextidx; 
  int valid;
}recv_port[RECV_PORT_MAX];

static int recv_port_head = -1;

// return an unused recv_port index,
// fail to -1
int get_recv_port(void){
  acquire(&netlock);
  if(recv_port_head < 0){
    recv_port_head = 0;
    recv_port[0].valid = 1;
    recv_port[0].nextidx = -1;
    release(&netlock);
    return recv_port_head;
  }
  for(int i = 0; i < RECV_PORT_MAX; i++){
    if(recv_port[i].valid == 0){
      recv_port[i].nextidx = recv_port_head;
      recv_port[i].valid = 1;
      recv_port_head = i;
      release(&netlock);
      return recv_port_head;
    }
  }
  release(&netlock);
  return -1;
}

int recv_port2idx(int port){
  acquire(&netlock);
  int idx = recv_port_head;
  while(idx >= 0 && recv_port[idx].valid){
    if(recv_port[idx].port == port){
      release(&netlock);
      return idx;
    }
    idx = recv_port[idx].nextidx;
  }
  release(&netlock);
  return -1;
}

void
netinit(void)
{
  initlock(&netlock, "netlock");
  memset(recv_port, 0, sizeof(recv_port));
}


//
// bind(int port)
// prepare to receive UDP packets address to the port,
// i.e. allocate any queues &c needed.
//
uint64
sys_bind(void)
{
  //
  // Your code here.
  //

  // struct proc *p = myproc();
  int port;
  
  argint(0, &port);
  int idx = get_recv_port();
  if(idx == -1){
    return -1;
  }

  recv_port[idx].port = port;
  recv_port[idx].queue_head = 0;
  recv_port[idx].queue_tail = 0;
  recv_port[idx].queue_size = 0;
  
  // printf("syscall bind: proc %d bind to port %d\n", p->pid, port);
  return 0;
}

//
// unbind(int port)
// release any resources previously created by bind(port);
// from now on UDP packets addressed to port should be dropped.
//
uint64
sys_unbind(void)
{
  //
  // Optional: Your code here.
  //

  int port;
  argint(0, &port);
  int pre = -1, cur = recv_port_head;
  while(cur != -1){
    if(recv_port[cur].port == port){
      if(pre >= 0){
        recv_port[pre].nextidx = recv_port[cur].nextidx;
        recv_port[cur].valid = 0;
        return 0;
      }else{
        recv_port_head = recv_port[cur].nextidx;
        recv_port[cur].valid = 0;
        return 0;
      }
    }
    pre = cur;
    cur = recv_port[cur].nextidx;
  }

  return -1;
}

//
// recv(int dport, int *src, short *sport, char *buf, int maxlen)
// if there's a received UDP packet already queued that was
// addressed to dport, then return it.
// otherwise wait for such a packet.
//
// sets *src to the IP source address.
// sets *sport to the UDP source port.
// copies up to maxlen bytes of UDP payload to buf.
// returns the number of bytes copied,
// and -1 if there was an error.
//
// dport, *src, and *sport are host byte order.
// bind(dport) must previously have been called.
//
uint64
sys_recv(void)
{
  //
  // Your code here.
  //

  int dport;
  int *src;
  short *sport;
  char *buf;
  int maxlen;
  argint(0, &dport);
  argaddr(1, (uint64*)&src);
  argaddr(2, (uint64*)&sport);
  argaddr(3, (uint64*)&buf);
  argint(4, &maxlen);
  
  struct proc *p = myproc();
  
  int idx = recv_port2idx(dport);
  acquire(&netlock);
  // if(idx < 0 || (recv_port[idx].pid != p->pid)){
  if(idx < 0){
    printf("sys_recv: binding error: idx = %d, \n", idx);
    release(&netlock);
    return -1;
  }

  
  if(recv_port[idx].queue_size == 0){
    // printf("recv: sleep\n");
    sleep(&recv_port[idx], &netlock);
  }
  int len_payload = 0;
  if(recv_port[idx].queue_size > 0){
    struct eth * eth = (struct eth*)recv_port[idx].bufs[recv_port[idx].queue_head];
    recv_port[idx].queue_size -= 1;
    recv_port[idx].queue_head = (recv_port[idx].queue_head + 1) % RECV_QUEUE_SIZE;
    struct ip * ip = (struct ip*)(eth + 1);
    struct udp * udp = (struct udp*)(ip + 1);
    int t_src = ntohl(ip->ip_src);
    short t_sport = ntohs((short)udp->sport);
    if(copyout(p->pagetable, (uint64)src, (char*)&t_src, sizeof(int)) < 0 ||
      copyout(p->pagetable, (uint64)sport, (char*)&t_sport, sizeof(short)) < 0){
      kfree((void*)eth);
      release(&netlock);
      return -1;
    }
    // *src = ntohl(ip->ip_src);
    // *sport = ntohs(udp->sport);
    len_payload = ntohs(udp->ulen) - sizeof(struct udp);
    if(len_payload > maxlen){
      len_payload = maxlen;
    }
    // printf("recv: copy\n");
    if(copyout(p->pagetable, (uint64)buf, (char*)(udp + 1), len_payload) < 0){
      printf("recv: copy error\n");
      kfree((void*)eth);
      release(&netlock);
      return -1;
    }
    kfree((void*)eth);
  }
  release(&netlock);
  return len_payload;
}

// This code is lifted from FreeBSD's ping.c, and is copyright by the Regents
// of the University of California.
static unsigned short
in_cksum(const unsigned char *addr, int len)
{
  int nleft = len;
  const unsigned short *w = (const unsigned short *)addr;
  unsigned int sum = 0;
  unsigned short answer = 0;

  /*
   * Our algorithm is simple, using a 32 bit accumulator (sum), we add
   * sequential 16 bit words to it, and at the end, fold back all the
   * carry bits from the top 16 bits into the lower 16 bits.
   */
  while (nleft > 1)  {
    sum += *w++;
    nleft -= 2;
  }

  /* mop up an odd byte, if necessary */
  if (nleft == 1) {
    *(unsigned char *)(&answer) = *(const unsigned char *)w;
    sum += answer;
  }

  /* add back carry outs from top 16 bits to low 16 bits */
  sum = (sum & 0xffff) + (sum >> 16);
  sum += (sum >> 16);
  /* guaranteed now that the lower 16 bits of sum are correct */

  answer = ~sum; /* truncate to 16 bits */
  return answer;
}

//
// send(int sport, int dst, int dport, char *buf, int len)
//
uint64
sys_send(void)
{
  struct proc *p = myproc();
  int sport;
  int dst;
  int dport;
  uint64 bufaddr;
  int len;

  argint(0, &sport);
  argint(1, &dst);
  argint(2, &dport);
  argaddr(3, &bufaddr);
  argint(4, &len);

  int total = len + sizeof(struct eth) + sizeof(struct ip) + sizeof(struct udp);
  if(total > PGSIZE)
    return -1;

  char *buf = kalloc();
  if(buf == 0){
    printf("sys_send: kalloc failed\n");
    return -1;
  }
  memset(buf, 0, PGSIZE);

  struct eth *eth = (struct eth *) buf;
  memmove(eth->dhost, host_mac, ETHADDR_LEN);
  memmove(eth->shost, local_mac, ETHADDR_LEN);
  eth->type = htons(ETHTYPE_IP);

  struct ip *ip = (struct ip *)(eth + 1);
  ip->ip_vhl = 0x45; // version 4, header length 4*5
  ip->ip_tos = 0;
  ip->ip_len = htons(sizeof(struct ip) + sizeof(struct udp) + len);
  ip->ip_id = 0;
  ip->ip_off = 0;
  ip->ip_ttl = 100;
  ip->ip_p = IPPROTO_UDP;
  ip->ip_src = htonl(local_ip);
  ip->ip_dst = htonl(dst);
  ip->ip_sum = in_cksum((unsigned char *)ip, sizeof(*ip));

  struct udp *udp = (struct udp *)(ip + 1);
  udp->sport = htons(sport);
  udp->dport = htons(dport);
  udp->ulen = htons(len + sizeof(struct udp));

  char *payload = (char *)(udp + 1);
  if(copyin(p->pagetable, payload, bufaddr, len) < 0){
    kfree(buf);
    printf("send: copyin failed\n");
    return -1;
  }

  e1000_transmit(buf, total);

  return 0;
}

void
ip_rx(char *buf, int len)
{
  // don't delete this printf; make grade depends on it.
  static int seen_ip = 0;
  if(seen_ip == 0)
    printf("ip_rx: received an IP packet\n");
  seen_ip = 1;

  //
  // Your code here.
  //

  struct eth *eth = (struct eth*)buf;
  struct ip *ip = (struct ip*)(eth + 1);
  if(ip->ip_p != IPPROTO_UDP){
    // printf("ip_rx: not UDP, drop packet\n");
    kfree(buf);
    return;
  }
  struct udp *udp = (struct udp*)(ip + 1);
  uint16 dport = ntohs(udp->dport);
  int idx = recv_port2idx((int)dport);
  if(idx < 0 || (recv_port[idx].queue_size >= RECV_QUEUE_SIZE)){
    // printf("ip_rx: idx < 0 or queue size > 16\n");
    kfree(buf);
    return;
  }
  recv_port[idx].bufs[recv_port[idx].queue_tail] = buf;
  recv_port[idx].queue_size += 1;
  recv_port[idx].queue_tail = (recv_port[idx].queue_tail + 1) % RECV_QUEUE_SIZE;
  wakeup(&recv_port[idx]);
  // printf("ip_rx: done\n");
  return;
}

//
// send an ARP reply packet to tell qemu to map
// xv6's ip address to its ethernet address.
// this is the bare minimum needed to persuade
// qemu to send IP packets to xv6; the real ARP
// protocol is more complex.
//
void
arp_rx(char *inbuf)
{
  static int seen_arp = 0;

  if(seen_arp){
    kfree(inbuf);
    return;
  }
  printf("arp_rx: received an ARP packet\n");
  seen_arp = 1;

  struct eth *ineth = (struct eth *) inbuf;
  struct arp *inarp = (struct arp *) (ineth + 1);

  char *buf = kalloc();
  if(buf == 0)
    panic("send_arp_reply");
  
  struct eth *eth = (struct eth *) buf;
  memmove(eth->dhost, ineth->shost, ETHADDR_LEN); // ethernet destination = query source
  memmove(eth->shost, local_mac, ETHADDR_LEN); // ethernet source = xv6's ethernet address
  eth->type = htons(ETHTYPE_ARP);

  struct arp *arp = (struct arp *)(eth + 1);
  arp->hrd = htons(ARP_HRD_ETHER);
  arp->pro = htons(ETHTYPE_IP);
  arp->hln = ETHADDR_LEN;
  arp->pln = sizeof(uint32);
  arp->op = htons(ARP_OP_REPLY);

  memmove(arp->sha, local_mac, ETHADDR_LEN);
  arp->sip = htonl(local_ip);
  memmove(arp->tha, ineth->shost, ETHADDR_LEN);
  arp->tip = inarp->sip;

  e1000_transmit(buf, sizeof(*eth) + sizeof(*arp));

  kfree(inbuf);
}

void
net_rx(char *buf, int len)
{
  struct eth *eth = (struct eth *) buf;

  if(len >= sizeof(struct eth) + sizeof(struct arp) &&
     ntohs(eth->type) == ETHTYPE_ARP){
    arp_rx(buf);
  } else if(len >= sizeof(struct eth) + sizeof(struct ip) &&
     ntohs(eth->type) == ETHTYPE_IP){
    ip_rx(buf, len);
  } else {
    kfree(buf);
  }
}
