int FindClusterType:: Find_ClusterType_in_binarymap_Type15(ParamManage& test_manage, int &dd_total_cnt, int &big_dd_cnt,int dd_totla_limit)
{

    //______________________

    logMsg("step_1",0,0);
    int width = test_manage.width_;
    int height = test_manage.height_;
    int zwidth = width>>2;

    //GC10V0
    if(width<=0 || height<=0 /*|| crop_x!=0 || crop_y!=0 ||  map_src==nullptr*/)
    {
        g_str_msg_all[site_].append ("image size or spec is wrong!\n");
        return -1;
    }

    dd_total_cnt=0;
    big_dd_cnt=0;
//    /*int */otp_dd_total_cnt =0;
//    int otp_dd_limit_cnt = 500;
//    uchar *map_img=nullptr;
//    if(!Algorithm_all[site_].New_Pointer_Buf (&map_img,Result_area,Buf_uchar_8,width,height,1))
//    {
//        g_str_msg_all[site_]+="Algorithm init failed!";
//        return -1;
//    }
//    memcpy(map_img,map_src,width*height*sizeof(uchar));
    for(int i=0;i!=15;++i)
        otp_dd_type[i]=0;


    //type big DD
    bool bType_bigDD = false;
    logMsg("start find big dd 1",0,0);
    for (int i = 2; i < height-2; ++i)
    {
        for (int j = 2; j < width-2; ++j)
        {
            if(map_img[i * width + j] == 255)
            {
                int cnt =0;
                for(int ii = i-2; ii <= i+2; ii++)
                    for(int jj = j-2; jj <= j+2; jj++)
                        if(map_img[width*ii+jj] ==255) cnt++;

                if(cnt >= 16)
                {
                    big_dd_cnt++;
                    bType_bigDD = true;
                }
            }
        }
    }

    // 同通道4连点
    logMsg("start find big dd 2",0,0);
    for(int i=0;i!=height;i++)
    {
        for(int j=6;j!=width;j++)
        {
            if(map_img[i * width + j]==255  && map_img[i * width + j-2]==255 &&
               map_img[i * width + j-4]==255 && map_img[i * width + j-6]==255)
            {
                big_dd_cnt++;
                bType_bigDD = true;
                if(big_dd_cnt<=10)
                {
                    QString msg=QString("Find big dd x:%1,y%2 \n").arg(j).arg(i);
                    g_str_msg_all[site_].append (msg.toStdString());
                }
            }
        }
    }

    if(bType_bigDD )
        return 1;

    logMsg("start find cluster",0,0);


    // 使用 QPair 列表初始化 QMap
    // 创建一个空的 QMap
    QMap<unsigned char, int> cluster_type;

    // 手动插入元素
    cluster_type.insert(0b1000, 1);
    cluster_type.insert(0b0100, 2);
    cluster_type.insert(0b1100, 3);
    cluster_type.insert(0b0010, 4);
    cluster_type.insert(0b1010, 5);
    cluster_type.insert(0b0110, 6);
    cluster_type.insert(0b1110, 7);
    cluster_type.insert(0b0001, 8);
    cluster_type.insert(0b1001, 9);
    cluster_type.insert(0b0101, 10);
    cluster_type.insert(0b1101, 11);
    cluster_type.insert(0b0011, 12);
    cluster_type.insert(0b1011, 13);
    cluster_type.insert(0b0111, 14);
    cluster_type.insert(0b1111, 15);



    //双坏点及以上
    int cluster_cnt=0;
    QVector<uchar> type_map(width*height/4,0);
    for(int i=0,j=0;i!=width*height;i=i+4,j++)
    {
        int sum = map_img[i]+map_img[i+1]+map_img[i+2]+map_img[i+3];
        if(sum>255)
        {
            // 相邻双坏点不处理-dpc on可消
            if( (map_img[i]==255 && map_img[i+1]==255) || (map_img[i+1]==255 && map_img[i+2]==255) || (map_img[i+2]==255 && map_img[i+3]==255) )
            {
                //g_str_msg_all[site_].append (boost::str(boost::format("Find cluster dd excexpt x = %d,y = %d  \n")%(i%width)%(i/width)));
                continue;
            }

            cluster_cnt++;
            type_map[j]= (map_img[i]&0b1000) | (map_img[i+1]&0b0100) | (map_img[i+2]&0b0010) | (map_img[i+3]&0b0001);
            map_img[i] = map_img[i+1] = map_img[i+2] = map_img[i+3]=0;
            if(cluster_cnt<10)
            {
                int type_=cluster_type[type_map[j]];
                g_str_msg_all[site_].append (boost::str(boost::format("Find cluster dd x = %d,y = %d ,type= %x \n")%(i%width)%(i/width)%(type_)));


            }


        }
    }



    // 创建一个空的 QSet<unsigned char>
    QSet<unsigned char> type_1000;

    // 使用 insert() 插入元素
    type_1000.insert(0b1010);
    type_1000.insert(0b1110);
    type_1000.insert(0b1011);
    type_1000.insert(0b1111);


    //----------------------------
    QSet<uchar> type_0100;
    type_0100.insert(0b0101);
    type_0100.insert(0b1101);
    type_0100.insert(0b0111);
    type_0100.insert(0b1111);

    //----------------------------
    QSet<uchar> type_0010;
    type_0010.insert(0b1100);
    type_0010.insert(0b1010);
    type_0010.insert(0b1001);
    type_0010.insert(0b1011);
    type_0010.insert(0b1101);
    type_0010.insert(0b1110);
    type_0010.insert(0b1111);

    //___________________________
    QSet<uchar> type_0001;
    type_0001.insert(0b1100);
    type_0001.insert(0b0110);
    type_0001.insert(0b0101);
    type_0001.insert(0b0111);
    type_0001.insert(0b1101);
    type_0001.insert(0b1110);
    type_0001.insert(0b1111);


    //水平单坏点
    int k=0;
    int single_cnt=0;
    for(int i=0;i!=height;++i)
    {
        for(int j=0;j!=width;j=j+4,k++)
        {
            int loca=i*width+j;
            int sum = map_img[loca]+map_img[loca+1]+map_img[loca+2]+map_img[loca+3];

            if(sum==255)
            {
                if(map_img[loca]==255 && j>=4)
                {
                    if(type_1000.contains(type_map[k-1]))
                        type_map[k]=0b1000;
                }
                else if(map_img[loca+1]==255 && j>=4)
                {
                    if(type_0100.contains(type_map[k-1]))
                        type_map[k]=0b0100;
                }
                else if(map_img[loca+2]==255 && j+4<width)
                {
                    if(map_img[loca+4]==255 || type_0010.contains(type_map[k+1]))
                    {
                        type_map[k]=0b0010;
                    }
                }
                else if(map_img[loca+3]==255 && j+4<width)
                {
                    if(/*map_img[loca+4]==255 || */map_img[loca+5]==255 || type_0001.contains(type_map[k+1]))
                        type_map[k]=0b0001;
                }

                if(type_map[k]!=0)
                {
                    single_cnt++;
//                    map_img[i] = map_img[i+1] = map_img[i+2] = map_img[i+3]=0;
                    map_img[loca] = map_img[loca+1] = map_img[loca+2] = map_img[loca+3] = 0;
                    if(single_cnt<10)
                    {
                        int type_=cluster_type[type_map[k]];
                        g_str_msg_all[site_].append (boost::str(boost::format("Find single_1 dd x = %d,y = %d ,type= %x \n")%(j)%(i)%(type_)));

                    }

                }
            }
        }
    }


    //垂直单坏点
    QMap<int,uchar>single_map;
    single_map.insert(0,0b1000);
    single_map.insert(1,0b0100);
    single_map.insert(2,0b0010);
    single_map.insert(3,0b0001);

    for(int i=0;i!=height;++i)
    {
        for(int j=0;j!=width;++j)
        {
            if(map_img[i * width + j]==255)
            {
                bool need_burn=false;
                if(i+1!=height)
                {
                    if(/*map_img[ (i+1) * width + j]==255 || */(j-1>=0 && map_img[ (i+1) * width + j-1]==255) || (j+1!=width && map_img[ (i+1) * width + j+1]==255) )
                        need_burn=true;
                }
                if(!need_burn && i+2!=height)
                {
                    if(map_img[ (i+2) * width + j]==255 || (j-2>=0 && map_img[ (i+2) * width + j-2]==255) || (j+2!=width && map_img[ (i+2) * width + j+2]==255) )
                        need_burn=true;
                }

                if(need_burn)
                {
                    single_cnt++;
                    int loca=i * width + j;
                    map_img[loca]=0;
                    type_map[loca/4]=single_map[loca%4];
                    if(single_cnt<10)
                    {
                        int type_=cluster_type[type_map[loca/4]];

                        g_str_msg_all[site_].append (boost::str(boost::format("Find single_2 dd x = %d,y = %d,type =%x \n")%(4*j)%(i)%(type_)));
                    }
                }
            }
        }
    }

    //write dd info
//    int dd_info_cnt=0;
//    dd_total_cnt=cluster_cnt+single_cnt;
    if (1/*dd_total_cnt <= dd_totla_limit*/)
    {
        for(int i =0;i<height;i++)
        {
            for(int j =0;j< zwidth;j++)
            {
                uchar type_ = type_map[i*zwidth+j];
                if(cluster_type.contains(type_))
                {
                    int otp_type = cluster_type[type_];
                    otp_dd_type[otp_type-1]++;

//                    int col = j+0+1;
//                    int row = i+0+1;

//                    dd_info[dd_info_cnt++] = (col & 0x00ff);
//                    dd_info[dd_info_cnt++] = (row & 0x000f) << 4 | ((col >>8) &0x0007);
//                    dd_info[dd_info_cnt++] = (row >>4) & 0x00ff;
//                    dd_info[dd_info_cnt++] = otp_type&0x0f;

//                    dd_info_xy.insert(QPair<int,int>(i,j*4),otp_type&0x0f);
                    dd_info_xy.insert(QPair<int,int>(i,j),otp_type&0x0f);
                }
            }
        }
    }

    // Fix: dd_total_cnt 直接使用 dd_info_xy 的数量，保证两者一致
    dd_total_cnt = dd_info_xy.size();

    return 1;


}
