This document will describe some important things for design DMS clientnode. It should be continuous update.

1. Request for Metadata Protocol between CN and NN
Request: from CN to NN

--------------------------------------------------------------------------------
| magic # | rw      | start LB | request id | volume id | LB length | magic #  |
| 8 bytes | 2 bytes | 8 bytes  | 8 bytes    | 8 bytes   |  8 bytes  |  8 bytes |
--------------------------------------------------------------------------------

rw = 0 (Request Metadata for Read)
rw = 1 (Request True Metadata for Read)
rw = 2 (Request namenode allocate metadata and get old metadata)
rw = 3 (Request namenode allocate metadata without get old metadata)
rw = 4 (Request True Metadata for Write)

Response: from NN to CN
Type 1: Metadata Response - Response write with/without get old metadata or Response for Query Metadata for Read/Write. Format as following:
The # of lr will >= 2 with old metadata and lr[0] will be new metadata, lr[1] ... lr[# of lr - 1] will be old metadata
The # of lr will == 1 without old metadata and lr[0] will be new metadata

pkt len = total response size - size of magic # - size of pkt len
--------------------------------------------------------------------------------------------------------
| magic # | pkt len | creq id | nres_id | # of lr | lr[0] | lr[1]  |........| lr[# of lr-1] | lr state |
| 8 bytes | 2 bytes | 8 bytes | 8 bytes | 2 bytes |       |        |        |               | 2 bytes  |
--------------------------------------------------------------------------------------------------------

lr (located request) format
------------------------------------------------------------------------------------------------------------------------------------------------------
| len of hbids | hbids[0] | hbids[1] | ........ | hbids[len_of_hbids-1] | len of dn_loc |  dn loc[0] | dn_loc[1] | ....... | dn_loc[len of dn_loc-1] |
|  2 bytes     |  8 bytes | 8 bytes  |          | 8 bytes               | 2 bytes       |            |           |         |                         |
------------------------------------------------------------------------------------------------------------------------------------------------------

dn_loc (datanode location information) format
-----------------------------------------------------------------------------------------------------------------
| len of hostname |  hostname   | port    | # of phy locs | rlo[0]  | rlo[1]  |........| rlo[# of phy locs - 1] |
|  2 bytes        |  char array | 4 bytes |   2 bytes     | 8 bytes | 8 bytes |        |  8 bytes               |
-----------------------------------------------------------------------------------------------------------------

rlo: composited by 3 data: RBID, length, offset.

Special NOTE:  The total number of HBID should equals to the sum of length in rlo[0].length + rlo[1].length + ... + rlo[# of phy locs - 1].length

Consideration following case:
Query to Namenode with startLB = 1 and LB_cnt = 10, the response will look like following:

----------------------------------------------------
| magic # | pkt len | creq id | nres_id | # of lr  | 
| 8 bytes | 2 bytes | 8 bytes | 8 bytes | = 7      | 
----------------------------------------------------

LR[0]: hb[0], hb[1] for LB[0] LB[1]
----------------------------------------------------------------------------------------
| # of hb | hb1 | hb2 |  len of dnloc | dn_loc[0] = DN1  |  dn_loc[1] = DN2 | hb state |      The length of sum in rlo1_1 = len of hb
|    2    |     |     |  = 2          | rlo1_1 data      |  rlo2_1 data     |          |      The length of sum in rlo2_1 = len of hb
----------------------------------------------------------------------------------------

LR[1]: hb[0] for LB[2]
---------------------------------------------------------------------------------------------------
| # of hb | hb3 | len of dnloc | dn_loc[0] = DN1  |  dn_loc[1] = DN2 | dn_loc[2] = DN3 | hb state |
|    1    |     | = 3          | rlo1_2 data      |  rlo2_2 data     | rlo3_1 data     |          |
---------------------------------------------------------------------------------------------------

LR[2]: hb[0] for LB[3] hb[1] for LB[4]
----------------------------------------------------------------------------------------
| # of hb | hb4  | hb5 | len of dnloc | dn_loc[0] = DN1  |  dn_loc[1] = DN3 | hb state |
|    2    |      |     | = 2          | rlo1_3 data      |  rlo3_2 data     |          |
----------------------------------------------------------------------------------------

LR[3]: hb[0] for LB[5]
---------------------------------------------------------------
| # of hb | hb6  | len of dnloc | dn_loc[0] = DN4  | hb state |
|    1    |      | = 1          | rlo4_1 data      |          |
---------------------------------------------------------------

LR[4]: hb[0] for LB[6]
---------------------------------------------------------------------------------------------------
| # of hb | hb7 | len of dnloc | dn_loc[0] = DN4  |  dn_loc[1] = DN5 | dn_loc[2] = DN6 | hb state |
|    1    |     | = 3          | rlo4_2 data      |  rlo5_1 data     | rlo6_1 data     |          |
---------------------------------------------------------------------------------------------------

LR[5]: hb[0] for LB[7]
---------------------------------------------------------------
| # of hb | hb8  | len of dnloc | dn_loc[0] = DN4  | hb state |
|    1    |      | = 1          | rlo5_2 data      |          |
---------------------------------------------------------------

LR[6]: hb[0] for LB[8], hb[1] for LB[9]
----------------------------------------------------------------------------------------------------------
| # of hb | hb9 | hb10 | len of dnloc | dn_loc[0] = DN1  |  dn_loc[1] = DN2 | dn_loc[2] = DN7 | hb state |
|    2    |     |      | = 3          | rlo1_4 data      |  rlo2_3 data     | rlo7_1 data     |          |
----------------------------------------------------------------------------------------------------------


******* NameNode will not generate following format: *************

LR[0]: hb[0] to hb[4] for LB[0] to LB[4]
---------------------------------------------------------------------------------------------------------------------------
| # of hb | hb1 | hb2 | hb3 | hb4 | hb5 |  len of dnloc | dn_loc[0] = DN1  |  dn_loc[1] = DN2 | dn_loc[2] = DN3 |hb state |
|    5    |     |     |     |     |     |  = 3          | rlo1_1 data      |  rlo2_1 data     | rlog3_1 data              |
---------------------------------------------------------------------------------------------------------------------------
The actually information is the sum of len in rlo1_1 data equals to 5 (# of hbs)
                                sum of len in rlo2_1 data == 3 (matach hb[0], hb[1], hb[2])
								sum of len in rlo3_1 data == 3 (matach hb[2], hb[3], hb[4])
The protocol cannot describe such information, so namenode won't reply this format.

LR[1]: hb[0] to hb[2] for LB[5] to LB[7]
--------------------------------------------------------------------------------------------------------------
| # of hb | hb1 | hb2 | hb3 | len of dnloc | dn_loc[0] = DN4  |  dn_loc[1] = DN5 | dn_loc[2] = DN6 |hb state |
|    3    |     |     |     |  = 3         | rlo4_1 data      |  rlo5_1 data     | rlog6_1 data    |         |
--------------------------------------------------------------------------------------------------------------
The actually information is the sum of len in rlo4_1 data equals to 2 (matach hb[0], hb[1])
                                sum of len in rlo5_1 data == 2 (matach hb[1], hb[2])
								sum of len in rlo6_1 data == 1 (matach hb[1])
The protocol cannot describe such information, so namenode won't reply this format.

LR[2]: hb[0] to hb[1] for LB[8] to LB[9]
--------------------------------------------------------------------------------------------------------
| # of hb | hb1 | hb2 | len of dnloc | dn_loc[0] = DN1  |  dn_loc[1] = DN2 | dn_loc[2] = DN7 |hb state |
|    2    |     |     |  = 3         | rlo1_2 data      |  rlo2_2 data     | rlog7_1 data    |         |
--------------------------------------------------------------------------------------------------------
The actually information is the sum of len in rlo1_2 data equals to 2 (matach hb[0], hb[1])
                                sum of len in rlo2_2 data == 2  (matach hb[0], hb[1])
								sum of len in rlo7_1 data == 1  (matach hb[0], hb[1])
The protocol can describe such information, so namenode will reply this format.

2. Report Metadat access status

---------------------------------------------------------------------------------------------------------------------------------
| magic # | opCode  | Commit_it | # of report item | report_item[0] | report_item[1] |........| report_item[# of report item -1 |
| 8 bytes | 2 bytes | 8 bytes   | 4 bytes          |                |                |        |                                 |
---------------------------------------------------------------------------------------------------------------------------------

opCode = 5 (Report fail location when read)
opCode = 7 (Report success location when overwrite)
opCode = 6 (Report success location when first write)

When opCode = 6
the format of report_item will be following:
---------------------------------------------------------------------------------------------------------
| hostname | port    | # of phy locs | Triplets[0] | Triplets[1] |........| Triplets[# of phy locs - 1] |
| 20 bytes | 4 bytes | 4 bytes       | 8 bytes     | 8 bytes     |        | 8 bytes                     |
---------------------------------------------------------------------------------------------------------

NOTE: If # of report item = 0, doesn't need report

When opCode = 5 or 7
the format of report_item will be following:
-----------------------------------------------------------------------------------------------------------------------------------------------------
| LB start | LB length | HBID[0] | HBID[1] | ........| HBID[LB length - 1] | # of dnlocs | dnlocs[0] | dnlocs[1] | ........| dnlocs[# of dnlocs -1] |
| 8 bytes  | 4 bytes   | 8 bytes | 8 bytes |         | 8 bytes             | 4 bytes     |           |           |         |                        |
-----------------------------------------------------------------------------------------------------------------------------------------------------

the dnlocs format will be following:
---------------------------------------------------------------------------------------------------------
| hostname | port    | # of phy locs | Triplets[0] | Triplets[1] |........| Triplets[# of phy locs - 1] |
| 20 bytes | 4 bytes | 4 bytes       | 8 bytes     | 8 bytes     |        | 8 bytes                     |
---------------------------------------------------------------------------------------------------------

NOTE: When opCode = 5, clientnode doesn't need report metadata access status if the # of report item == 0
      When opCode = 7, clientnode should report metadta access status if following criteria be meet:
	  S1: The replica of located requests < volume'e replica even all replica in located requests are all success for write
	  S2: If the # of dnlocs is 0, clientnode has report the write status (all replica are failed to write)
