#ifndef HASH_H
#define HASH_H

#include <linux/vmalloc.h>
#include <linux/slab.h>
#include "rule.h" //Get def of Rule

#define HASH_RANGE 1024
#define QUEUE_SIZE 128
//#define NULL 0

//Link Node of Rule
struct RuleNode{
        struct Rule r;         //content of rule
        struct RuleNode* next; //pointer to next node 
};

//Link List of Rules
struct RuleList{
        struct RuleNode* head; //pointer to head node of this link list
        int len;               //current length of this list (max: QUEUE_SIZE)
};

//Hash Table of Rules
struct RuleTable{
        struct RuleList* table; //many RuleList (HASH_RANGE)
        int size;               //total number of nodes in this table
};

//Hash function, calculate the rule should be inserted into which RuleList
static unsigned int Hash(struct Rule* r)
{
        //return a value in [0,HASH_RANGE-1]
        //src_ip dst_ip protocol src_port dst_port (5 tuples to determine a flow) and direction
       return ((r->direction+1)*(r->src_ip/(256*256*256)+1)*(r->dst_ip/(256*256*256)+1)*(r->src_port+1)*(r->dst_port+3)*(r->protocol+5))%HASH_RANGE;
}

//Determine whether two Rules are equal (same flow) 
static int Equal(struct Rule* r1,struct Rule* r2)
{
	return (r1->direction==r2->direction)&&
           (r1->src_ip==r2->src_ip)&&
           (r1->dst_ip==r2->dst_ip)&&
           (r1->protocol==r2->protocol)&&
           (r1->src_port==r2->src_port)&&
           (r1->dst_port==r2->dst_port);

        //return (r1->direction==r2->direction)&&
        //        ((r1->src_ip==r2->src_ip)||r1->src_ip==0||r2->src_ip==0)&&
        //        ((r1->dst_ip==r2->dst_ip)||r1->dst_ip==0||r2->dst_ip==0)&&
        //        ((r1->protocol==r2->protocol)||r1->protocol==0||r2->protocol==0)&&
        //        ((r1->src_port==r2->src_port)||r1->src_port==0||r2->src_port==0)&&
        //        ((r1->dst_port==r2->dst_port)||r1->dst_port==0||r2->dst_port==0);
}

//Initialize a Action
static void Init_Action(struct Action* a)
{
        a->new_ip=0;
        a->mark=0;
        a->dscp=0;
}

//Initialize a Rule
static void Init_Rule(struct Rule* r)
{
        r->direction=0;
        r->src_ip=0;
        r->dst_ip=0;
        r->src_port=0;
        r->dst_port=0;
        r->protocol=0;
        Init_Action(&(r->a));
}

//Initialize a RuleNode
static void Init_Node(struct RuleNode* rn)
{
        //Initialize next pointer as null
        rn->next=NULL;
        Init_Rule(&(rn->r));
}

//Initialize a RuleList
static void Init_List(struct RuleList* rl)
{
        //No nodes in current list
        rl->len=0;
        rl->head=vmalloc(sizeof(struct RuleNode));
        //rl->head=(struct RuleNode*)vmalloc(sizeof(struct RuleNode));
        //Initialize the header node of this RuleList as a null node
        Init_Node(rl->head);
}

//Initialize a RuleTable
static void Init_Table(struct RuleTable* rt)
{
        int i=0;
        //allocate space for RuleLists
        rt->table=vmalloc(HASH_RANGE*sizeof(struct RuleList));
        //rt->table=(struct RuleList*)vmalloc(HASH_RANGE*sizeof(struct RuleList));
        for(i=0;i<HASH_RANGE;i++)
        {
                //Initialize each RuleList
                Init_List(&(rt->table[i]));
        }
        //No nodes in current table
        rt->size=0;
        //printk(KERN_INFO "Initialize RuleTable\n");
}

//Insert a Rule into a RuleList
//If the new rule is inserted successfully, return 1
//Else (e.g. rl->len>=QUEUE_SIZE or the same rule exists), return 0
static int Insert_List(struct RuleList* rl, struct Rule* r, int flags)
{
	if(rl->len>=QUEUE_SIZE) 
	{
		//printk(KERN_INFO "No enough space in this RuleList\n");
        return 0;
	} 
	else 
	{
		//printk(KERN_INFO "Start Insert_List\n");
        struct RuleNode* tmp=rl->head;
		struct RuleNode* buf=NULL;
        //Find the tail of this RuleList
        while(1)
        {
			//If pointer to next node is NULL, we find the tail of this RuleList. Here we can insert our new Rule
            if(tmp->next==NULL)
            {
				//printk(KERN_INFO "Come to the tail of RuleList\n");
                //Allocate space for new RuleNode
                //tmp->next=(struct RuleNode*)vmalloc(sizeof(struct RuleNode));
				buf=kmalloc(sizeof(struct RuleNode),flags);
				if(buf==NULL)
				{
					printk(KERN_INFO "Kmalloc error\n");
					return 0;
				}
				else
                {
					tmp->next=buf;
					//Copy data for this new RuleNode
					tmp->next->r=*r;
					//Pointer to next RuleNode is NUll
					tmp->next->next=NULL;
					//Increase length of RuleList
					rl->len++;
					//Finish the insert
					return 1;
				}
			}
			//If the rule of next node is the same as our inserted rule, we just replace the old rule rather than add new rule  
			else if(Equal(&(tmp->next->r),r))
			{
				//Update action of old rule to that of new rule
				tmp->next->r.a.mark=r->a.mark;
                tmp->next->r.a.new_ip=r->a.new_ip;
                tmp->next->r.a.dscp=r->a.dscp;
				return 0;
			}
			//Move to next RuleNode
            else
            {
				tmp=tmp->next;
            }
       }
	}
	return 0;
}

//Insert a rule to RuleTable
static void Insert_Table(struct RuleTable* rt,struct Rule* r, int flags)
{
		int result=0;
        unsigned int index=Hash(r);
        //printk(KERN_INFO "Insert to RuleList:%u\n",index);
        //Insert Rule to appropriate RuleList based on Hash value
        result=Insert_List(&(rt->table[index]),r, flags);
        //Increase the size of RuleTable
        rt->size+=result;
        //printk(KERN_INFO "Insert complete\n");
}

//Search a action for a given rule in a RuleList
static struct Action Search_List(struct RuleList* rl, struct Rule* r)
{
        struct Action a;
        Init_Action(&a);

        //The length of RuleList is 0
        if(rl->len==0) 
		{
                //printf("No RuleNode in this RuleList to delete\n");
                //return NULL Action
                return a;

        } else {
                
                struct RuleNode* tmp=rl->head;
                //Find the Rule in this RuleList
                while(1)
                {
                        //If pointer to next node is NULL, we find the tail of this RuleList, no more RuleNodes to seach
                        if(tmp->next==NULL)
                        {
                                //return NULL Action
                                return a;
                        }
                        //Find matching rule (matching RuleNode is tmp->next rather than tmp)
                        if(Equal(&(tmp->next->r),r))//tmp->next->r==(*r))
                        {
                                //return Action of this matching rule
                                a.mark=tmp->next->r.a.mark;
                                a.new_ip=tmp->next->r.a.new_ip;
                                a.dscp=tmp->next->r.a.dscp;
                                return a;
                        }
                        else
                        {
                                //Move to next RuleNode
                                tmp=tmp->next;
                        }
                }
        }
        return a;
}

//Search a action for a given rule in a RuleTable
static struct Action Search_Table(struct RuleTable* rt, struct Rule* r)
{
        unsigned int index=0;
        index=Hash(r);
        return Search_List(&(rt->table[index]),r);
}

//Update a old rule to a new rule in a RuleList (these two rules must represnt ths same flow) 
static void Update_List(struct RuleList* rl, struct Rule* r)
{

        //The length of RuleList is zero
        if(rl->len==0) {

                        //printf("No RuleNode in this RuleList to delete\n");
                return;

        } else {
                
                struct RuleNode* tmp=rl->head;
                //Find oldr in this RuleList
                while(1)
                {
                        //If pointer to next node is NULL, we find the tail of this RuleList, no more RuleNodes to seach
                        if(tmp->next==NULL)
                        {
                                //Terminate the update process
                                return;
                        }
                        //Find matching rule (oldr) (matching RuleNode is tmp->next rather than tmp)
                        if(Equal(&(tmp->next->r),r))//tmp->next->r==(*r))
                        {
                                //Update action of old rule to that of new rule
                                tmp->next->r.a.mark=r->a.mark;
                                tmp->next->r.a.new_ip=r->a.new_ip;
                                tmp->next->r.a.dscp=r->a.dscp;
                                return;
                        }
                        else
                        {
                                //Move to next RuleNode
                                tmp=tmp->next;
                        }
                }
        }
}

//Update a old rule to a new rule in a RuleTable (these two rules must represnt ths same flow) 
static void Update_Table(struct RuleTable* rt, struct Rule* r)
{

        unsigned int index=0;
        index=Hash(r);
        //Update oldr to newr in an appropriate RuleList
        Update_List(&(rt->table[index]),r);
}

//Delete a rule from RuleList
//If the rule is deleted successfully, return 1
//Else, return 0
static int Delete_List(struct RuleList* rl, struct Rule* r)
{
        if(rl->len==0) {

                //printf("No RuleNode in this RuleList to delete\n");
                return 0;

        } else {
                
                struct RuleNode* tmp=rl->head;
                //Find the tail of this RuleList
                while(1)
                {
                        //If pointer to next node is NULL, we find the tail of this RuleList, no more RuleNodes, return 0
                        if(tmp->next==NULL)
                        {
                                return 0;
                        }
                        //Find matching rule (matching RuleNode is tmp->next rather than tmp), delete rule and return 1
                        if(Equal(&(tmp->next->r),r))
                        {
                                struct RuleNode* s=tmp->next;
                                tmp->next=s->next;
                                //Delete matching RuleNode from this RuleList
                                kfree(s);
                                //Decrease the length of this RuleList by one
                                rl->len--;
                                return 1;
                        }
                        else
                        {
                                //Move to next RuleNode
                                tmp=tmp->next;
                        }
                }
        }
		return 0;
}

static void Delete_Table(struct RuleTable* rt,struct Rule* r)
{
		int result=0;
        unsigned int index=0;
        index=Hash(r);
        //Delete Rule from appropriate RuleList based on Hash value
        result=Delete_List(&(rt->table[index]),r);
        //Decrease the size of RuleTable by one
        rt->size-=result;
}

static void Empty_List(struct RuleList* rl)
{
        struct RuleNode* NextNode;
        struct RuleNode* Ptr;
        for(Ptr=rl->head;Ptr!=NULL;Ptr=NextNode)
        {
                NextNode=Ptr->next;
				if(Ptr==rl->head)
                {
					vfree(Ptr);
				}
				else
				{
					kfree(Ptr);
				}
        }
        //vfree(rl->head);
        //rl->head=NULL;

}

static void Empty_Table(struct RuleTable* rt)
{
        int i=0;
        for(i=0;i<HASH_RANGE;i++)
        {
                Empty_List(&(rt->table[i]));
        }
        vfree(rt->table);
}

//Print a RuleNode
static void Print_Node(struct RuleNode* rn)
{
	  char src_ip[16]={0};           //source ip address 
	  char dst_ip[16]={0};           //destination ip address 
	  char new_ip[16]={0};           //new destination ip address

	  snprintf(src_ip, 16, "%pI4", &(rn->r.src_ip));
      snprintf(dst_ip, 16, "%pI4", &(rn->r.dst_ip));
	  snprintf(new_ip, 16, "%pI4", &(rn->r.a.new_ip));

	  //printk(KERN_INFO "ditrction:%u src_ip:%x dst_ip:%x src_port:%u dst_port:%u protocol:%u new_ip:%x mark:%u dscp:%u\n",rn->r.direction,rn->r.src_ip,rn->r.dst_ip,rn->r.src_port,rn->r.dst_port,rn->r.protocol,rn->r.a.new_ip,rn->r.a.mark,rn->r.a.dscp);
	  printk(KERN_INFO "ditrction:%u src_ip:%s dst_ip:%s src_port:%u dst_port:%u protocol:%u new_ip:%s mark:%u dscp:%u\n",rn->r.direction,src_ip,dst_ip,rn->r.src_port,rn->r.dst_port,rn->r.protocol,new_ip,rn->r.a.mark,rn->r.a.dscp);

}

//Print a RuleList
static void Print_List(struct RuleList* rl)
{
        struct RuleNode* Ptr;
        for(Ptr=rl->head->next;Ptr!=NULL;Ptr=Ptr->next)
        {
                Print_Node(Ptr);
        }
}

//Print a RuleTable
static void Print_Table(struct RuleTable* rt)
{
        int i=0;
		printk(KERN_INFO "Current rule table:\n");
        for(i=0;i<HASH_RANGE;i++)
        {
                if(rt->table[i].len>0)
                {
                        printk(KERN_INFO "RuleList %d\n",i);
                        Print_List(&(rt->table[i]));
                        //printk(KERN_INFO "\n");
                }
        }
        printk(KERN_INFO "There are %d rules in total\n",rt->size);
}

#endif