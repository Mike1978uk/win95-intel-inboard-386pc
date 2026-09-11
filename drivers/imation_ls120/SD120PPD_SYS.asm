2074  1e                   push ds                         
2075  e8 5f 00             call 0x20d7                     
2078  89 1e 66 0a          mov word ptr [0xa66], bx        
207C  8c 06 68 0a          mov word ptr [0xa68], es        
2080  1f                   pop ds                          
2081  cb                   retf                            
2082  50                   push ax                         
2083  53                   push bx                         
2084  51                   push cx                         
2085  52                   push dx                         
2086  56                   push si                         
2087  57                   push di                         
2088  55                   push bp                         
2089  06                   push es                         
208A  1e                   push ds                         
208B  e8 49 00             call 0x20d7                     
208E  c4 1e 66 0a          les bx, ptr [0xa66]             
2092  26 8a 47 02          mov al, byte ptr es:[bx + 2]    
2096  0a c0                or al, al                       
2098  74 20                je 0x20ba                       
209A  3c 03                cmp al, 3                       
209C  74 24                je 0x20c2                       
209E  3c 18                cmp al, 0x18                    
20A0  b8 03 81             mov ax, 0x8103                  
20A3  77 03                ja 0x20a8                       
20A5  b8 00 01             mov ax, 0x100                   
20A8  c4 1e 66 0a          les bx, ptr [0xa66]             
20AC  26 89 47 03          mov word ptr es:[bx + 3], ax    
20B0  1f                   pop ds                          
20B1  07                   pop es                          
20B2  5d                   pop bp                          
20B3  5f                   pop di                          
20B4  5e                   pop si                          
20B5  5a                   pop dx                          
20B6  59                   pop cx                          
20B7  5b                   pop bx                          
20B8  58                   pop ax                          
20B9  cb                   retf                            
20BA  e8 2c 61             call 0x81e9                     
20BD  e8 f5 61             call 0x82b5                     
20C0  eb ee                jmp 0x20b0                      
20C2  26 c4 7f 0e          les di, ptr es:[bx + 0xe]       
20C6  1e                   push ds                         
20C7  e8 4a 3b             call 0x5c14                     
20CA  26 89 35             mov word ptr es:[di], si        
20CD  26 8c 5d 02          mov word ptr es:[di + 2], ds    
20D1  1f                   pop ds                          
20D2  b8 00 01             mov ax, 0x100                   
20D5  eb d1                jmp 0x20a8                      
20D7  50                   push ax                         
20D8  8c c8                mov ax, cs                      
20DA  8e d8                mov ds, ax                      
20DC  58                   pop ax                          
20DD  c3                   ret                             
20DE  50                   push ax                         
20DF  53                   push bx                         
20E0  51                   push cx                         
20E1  52                   push dx                         
20E2  56                   push si                         
20E3  57                   push di                         
20E4  55                   push bp                         
20E5  06                   push es                         
20E6  1e                   push ds                         
20E7  bd 00 00             mov bp, 0                       
20EA  eb 2a                jmp 0x2116                      
20EC  50                   push ax                         
20ED  53                   push bx                         
20EE  51                   push cx                         
20EF  52                   push dx                         
20F0  56                   push si                         
20F1  57                   push di                         
20F2  55                   push bp                         
20F3  06                   push es                         
20F4  1e                   push ds                         
20F5  bd 01 00             mov bp, 1                       
20F8  eb 1c                jmp 0x2116                      
20FA  50                   push ax                         
20FB  53                   push bx                         
20FC  51                   push cx                         
20FD  52                   push dx                         
20FE  56                   push si                         
20FF  57                   push di                         
2100  55                   push bp                         
2101  06                   push es                         
2102  1e                   push ds                         
2103  bd 02 00             mov bp, 2                       
2106  eb 0e                jmp 0x2116                      
2108  50                   push ax                         
2109  53                   push bx                         
210A  51                   push cx                         
210B  52                   push dx                         
210C  56                   push si                         
210D  57                   push di                         
210E  55                   push bp                         
210F  06                   push es                         
2110  1e                   push ds                         
2111  bd 03 00             mov bp, 3                       
2114  eb 00                jmp 0x2116                      
2116  e8 be ff             call 0x20d7                     
2119  3e 8a 8e 6e 0a       mov cl, byte ptr ds:[bp + 0xa6e]
211E  80 f9 ff             cmp cl, 0xff                    
2121  74 26                je 0x2149                       
2123  e8 fd 01             call 0x2323                     
2126  72 21                jb 0x2149                       
2128  8b dd                mov bx, bp                      
212A  d1 e3                shl bx, 1                       
212C  83 bf 86 0a 00       cmp word ptr [bx + 0xa86], 0    
2131  74 11                je 0x2144                       
2133  fa                   cli                             
2134  ff 97 86 0a          call word ptr [bx + 0xa86]      
2138  83 f8 01             cmp ax, 1                       
213B  75 0c                jne 0x2149                      
213D  9c                   pushf                           
213E  8b b7 76 0a          mov si, word ptr [bx + 0xa76]   
2142  ff 1c                lcall [si]                      
2144  8b dd                mov bx, bp                      
2146  e8 54 02             call 0x239d                     
2149  1f                   pop ds                          
214A  07                   pop es                          
214B  5d                   pop bp                          
214C  5f                   pop di                          
214D  5e                   pop si                          
214E  5a                   pop dx                          
214F  59                   pop cx                          
2150  5b                   pop bx                          
2151  58                   pop ax                          
2152  cf                   iret                            
2153  e8 06 00             call 0x215c                     
2156  e8 2b 00             call 0x2184                     
2159  e2 f8                loop 0x2153                     
215B  c3                   ret                             
215C  50                   push ax                         
215D  9c                   pushf                           
215E  fa                   cli                             
215F  e4 61                in al, 0x61                     
2161  eb 00                jmp 0x2163                      
2163  24 fc                and al, 0xfc                    
2165  e6 61                out 0x61, al                    
2167  eb 00                jmp 0x2169                      
2169  b0 b4                mov al, 0xb4                    
216B  e6 43                out 0x43, al                    
216D  eb 00                jmp 0x216f                      
216F  b0 00                mov al, 0                       
2171  e6 42                out 0x42, al                    
2173  eb 00                jmp 0x2175                      
2175  e6 42                out 0x42, al                    
2177  eb 00                jmp 0x2179                      
2179  e4 61                in al, 0x61                     
217B  eb 00                jmp 0x217d                      
217D  0c 01                or al, 1                        
217F  e6 61                out 0x61, al                    
2181  9d                   popf                            
2182  58                   pop ax                          
2183  c3                   ret                             
2184  50                   push ax                         
2185  9c                   pushf                           
2186  fa                   cli                             
2187  e4 42                in al, 0x42                     
2189  8a e0                mov ah, al                      
218B  eb 00                jmp 0x218d                      
218D  e4 42                in al, 0x42                     
218F  9d                   popf                            
2190  86 c4                xchg ah, al                     
2192  f7 d0                not ax                          
2194  40                   inc ax                          
2195  3d a6 04             cmp ax, 0x4a6                   
2198  72 eb                jb 0x2185                       
219A  58                   pop ax                          
219B  c3                   ret                             
219C  06                   push es                         
219D  55                   push bp                         
219E  53                   push bx                         
219F  56                   push si                         
21A0  9c                   pushf                           
21A1  80 fb 04             cmp bl, 4                       
21A4  73 49                jae 0x21ef                      
21A6  32 ff                xor bh, bh                      
21A8  8b eb                mov bp, bx                      
21AA  80 bf 6e 0a ff       cmp byte ptr [bx + 0xa6e], 0xff 
21AF  b8 ff ff             mov ax, 0xffff                  
21B2  74 3b                je 0x21ef                       
21B4  c6 87 6e 0a ff       mov byte ptr [bx + 0xa6e], 0xff 
21B9  80 bf 72 0a 00       cmp byte ptr [bx + 0xa72], 0    
21BE  74 03                je 0x21c3                       
21C0  e8 b9 01             call 0x237c                     
21C3  80 c1 08             add cl, 8                       
21C6  80 f9 0f             cmp cl, 0xf                     
21C9  76 03                jbe 0x21ce                      
21CB  80 c1 60             add cl, 0x60                    
21CE  d1 e5                shl bp, 1                       
21D0  3e c7 86 86 0a 00 00 mov word ptr ds:[bp + 0xa86], 0 
21D7  3e 8b b6 76 0a       mov si, word ptr ds:[bp + 0xa76]
21DC  52                   push dx                         
21DD  1e                   push ds                         
21DE  8b 14                mov dx, word ptr [si]           
21E0  8b 44 02             mov ax, word ptr [si + 2]       
21E3  8e d8                mov ds, ax                      
21E5  b4 25                mov ah, 0x25                    
21E7  8a c1                mov al, cl                      
21E9  cd 21                int 0x21                        
21EB  1f                   pop ds                          
21EC  5a                   pop dx                          
21ED  33 c0                xor ax, ax                      
21EF  5b                   pop bx                          
21F0  f6 c7 02             test bh, 2                      
21F3  74 01                je 0x21f6                       
21F5  fb                   sti                             
21F6  5e                   pop si                          
21F7  5b                   pop bx                          
21F8  5d                   pop bp                          
21F9  07                   pop es                          
21FA  c3                   ret                             
21FB  c3                   ret                             
21FC  50                   push ax                         
21FD  53                   push bx                         
21FE  51                   push cx                         
21FF  52                   push dx                         
2200  56                   push si                         
2201  57                   push di                         
2202  55                   push bp                         
2203  06                   push es                         
2204  1e                   push ds                         
2205  e8 cf fe             call 0x20d7                     
2208  e8 19 fd             call 0x1f24                     
220B  50                   push ax                         
220C  9c                   pushf                           
220D  ff 1e 6a 0a          lcall [0xa6a]                   
2211  58                   pop ax                          
2212  0b c0                or ax, ax                       
2214  75 03                jne 0x2219                      
2216  e8 dc fc             call 0x1ef5                     
2219  e8 0a 00             call 0x2226                     
221C  1f                   pop ds                          
221D  07                   pop es                          
221E  5d                   pop bp                          
221F  5f                   pop di                          
2220  5e                   pop si                          
2221  5a                   pop dx                          
2222  59                   pop cx                          
2223  5b                   pop bx                          
2224  58                   pop ax                          
2225  cf                   iret                            
2226  56                   push si                         
2227  57                   push di                         
2228  8b 36 90 0a          mov si, word ptr [0xa90]        
222C  bf 90 0a             mov di, 0xa90                   
222F  0b f6                or si, si                       
2231  74 41                je 0x2274                       
2233  83 44 06 37          add word ptr [si + 6], 0x37     
2237  83 54 08 00          adc word ptr [si + 8], 0        
223B  8b 44 04             mov ax, word ptr [si + 4]       
223E  39 44 08             cmp word ptr [si + 8], ax       
2241  77 08                ja 0x224b                       
2243  8b 44 02             mov ax, word ptr [si + 2]       
2246  39 44 06             cmp word ptr [si + 6], ax       
2249  72 24                jb 0x226f                       
224B  c7 44 06 00 00       mov word ptr [si + 6], 0        
2250  c7 44 08 00 00       mov word ptr [si + 8], 0        
2255  ff 54 0a             call word ptr [si + 0xa]           ; 000A='SCSIMGR'
2258  80 7c 0e 01          cmp byte ptr [si + 0xe], 1      
225C  74 0c                je 0x226a                       
225E  8b 44 10             mov ax, word ptr [si + 0x10]    
2261  89 05                mov word ptr [di], ax           
2263  c7 44 0a 00 00       mov word ptr [si + 0xa], 0         ; 000A='SCSIMGR'
2268  eb 05                jmp 0x226f                      
226A  8b fe                mov di, si                      
226C  83 c7 10             add di, 0x10                    
226F  8b 74 10             mov si, word ptr [si + 0x10]    
2272  eb bb                jmp 0x222f                      
2274  5f                   pop di                          
2275  5e                   pop si                          
2276  c3                   ret                             
2277  56                   push si                         
2278  53                   push bx                         
2279  57                   push di                         
227A  8b 36 90 0a          mov si, word ptr [0xa90]        
227E  bf 90 0a             mov di, 0xa90                   
2281  0b f6                or si, si                       
2283  74 1f                je 0x22a4                       
2285  39 44 14             cmp word ptr [si + 0x14], ax    
2288  75 0c                jne 0x2296                      
228A  8b 44 10             mov ax, word ptr [si + 0x10]    
228D  89 05                mov word ptr [di], ax           
228F  c7 44 0a 00 00       mov word ptr [si + 0xa], 0         ; 000A='SCSIMGR'
2294  eb 05                jmp 0x229b                      
2296  8b fe                mov di, si                      
2298  83 c7 10             add di, 0x10                    
229B  8b 74 10             mov si, word ptr [si + 0x10]    
229E  eb e1                jmp 0x2281                      
22A0  5f                   pop di                          
22A1  5b                   pop bx                          
22A2  5e                   pop si                          
22A3  c3                   ret                             
22A4  06                   push es                         
22A5  53                   push bx                         
22A6  9c                   pushf                           
22A7  fa                   cli                             
22A8  33 db                xor bx, bx                      
22AA  8e c3                mov es, bx                      
22AC  8b 1e 6a 0a          mov bx, word ptr [0xa6a]        
22B0  53                   push bx                         
22B1  0b 1e 6c 0a          or bx, word ptr [0xa6c]         
22B5  5b                   pop bx                          
22B6  74 16                je 0x22ce                       
22B8  56                   push si                         
22B9  52                   push dx                         
22BA  1e                   push ds                         
22BB  be 6a 0a             mov si, 0xa6a                   
22BE  8b 14                mov dx, word ptr [si]           
22C0  8b 44 02             mov ax, word ptr [si + 2]       
22C3  8e d8                mov ds, ax                      
22C5  b4 25                mov ah, 0x25                    
22C7  b0 08                mov al, 8                       
22C9  cd 21                int 0x21                        
22CB  1f                   pop ds                          
22CC  5a                   pop dx                          
22CD  5e                   pop si                          
22CE  5b                   pop bx                          
22CF  f6 c7 02             test bh, 2                      
22D2  74 01                je 0x22d5                       
22D4  fb                   sti                             
22D5  5b                   pop bx                          
22D6  07                   pop es                          
22D7  c7 06 90 0a 00 00    mov word ptr [0xa90], 0         
22DD  eb c1                jmp 0x22a0                      
22DF  56                   push si                         
22E0  be 94 0a             mov si, 0xa94                   
22E3  83 7c 0a ff          cmp word ptr [si + 0xa], -1        ; 000A='SCSIMGR'
22E7  b8 ff ff             mov ax, 0xffff                  
22EA  74 0e                je 0x22fa                       
22EC  83 7c 0a 00          cmp word ptr [si + 0xa], 0         ; 000A='SCSIMGR'
22F0  b8 00 00             mov ax, 0                       
22F3  74 05                je 0x22fa                       
22F5  83 c6 16             add si, 0x16                    
22F8  eb e9                jmp 0x22e3                      
22FA  8c df                mov di, ds                      
22FC  8e c7                mov es, di                      
22FE  8b fe                mov di, si                      
2300  5e                   pop si                          
2301  c3                   ret                             
2302  56                   push si                         
2303  8b 36 90 0a          mov si, word ptr [0xa90]        
2307  81 3c 5a a5          cmp word ptr [si], 0xa55a       
230B  b8 ff ff             mov ax, 0xffff                  
230E  75 10                jne 0x2320                      
2310  83 7c 10 00          cmp word ptr [si + 0x10], 0     
2314  74 05                je 0x231b                       
2316  8b 74 10             mov si, word ptr [si + 0x10]    
2319  eb ec                jmp 0x2307                      
231B  89 7c 10             mov word ptr [si + 0x10], di    
231E  33 c0                xor ax, ax                      
2320  5e                   pop si                          
2321  c3                   ret                             
2322  c3                   ret                             
2323  52                   push dx                         
2324  50                   push ax                         
2325  ba 20 00             mov dx, 0x20                    
2328  80 f9 07             cmp cl, 7                       
232B  76 03                jbe 0x2330                      
232D  ba a0 00             mov dx, 0xa0                    
2330  b0 0b                mov al, 0xb                        ; 000B='CSIMGR'
2332  ee                   out dx, al                      
2333  b4 01                mov ah, 1                       
2335  d2 c4                rol ah, cl                      
2337  ec                   in al, dx                       
2338  84 c4                test ah, al                     
233A  b0 0a                mov al, 0xa                        ; 000A='SCSIMGR'
233C  ee                   out dx, al                      
233D  f9                   stc                             
233E  74 01                je 0x2341                       
2340  f8                   clc                             
2341  58                   pop ax                          
2342  5a                   pop dx                          
2343  c3                   ret                             
2344  c3                   ret                             
2345  51                   push cx                         
2346  e8 70 fa             call 0x1db9                     
2349  8a c8                mov cl, al                      
234B  e8 02 00             call 0x2350                     
234E  59                   pop cx                          
234F  c3                   ret                             
2350  b4 01                mov ah, 1                       
2352  d2 c4                rol ah, cl                      
2354  80 f9 07             cmp cl, 7                       
2357  77 0f                ja 0x2368                       
2359  80 f9 02             cmp cl, 2                       
235C  74 0a                je 0x2368                       
235E  e4 21                in al, 0x21                     
2360  f6 d4                not ah                          
2362  22 c4                and al, ah                      
2364  e6 21                out 0x21, al                    
2366  eb 08                jmp 0x2370                      
2368  e4 a1                in al, 0xa1                     
236A  f6 d4                not ah                          
236C  22 c4                and al, ah                      
236E  e6 a1                out 0xa1, al                    
2370  c3                   ret                             
2371  51                   push cx                         
2372  e8 44 fa             call 0x1db9                     
2375  8a c8                mov cl, al                      
2377  e8 02 00             call 0x237c                     
237A  59                   pop cx                          
237B  c3                   ret                             
237C  b4 01                mov ah, 1                       
237E  d2 c4                rol ah, cl                      
2380  80 f9 00             cmp cl, 0                       
2383  74 17                je 0x239c                       
2385  80 f9 07             cmp cl, 7                       
2388  77 0c                ja 0x2396                       
238A  80 f9 02             cmp cl, 2                       
238D  74 07                je 0x2396                       
238F  e4 21                in al, 0x21                     
2391  0a c4                or al, ah                       
2393  e6 21                out 0x21, al                    
2395  c3                   ret                             
2396  e4 a1                in al, 0xa1                     
2398  0a c4                or al, ah                       
239A  e6 a1                out 0xa1, al                    
239C  c3                   ret                             
239D  51                   push cx                         
239E  e8 18 fa             call 0x1db9                     
23A1  8a c8                mov cl, al                      
23A3  b0 20                mov al, 0x20                    
23A5  80 f9 07             cmp cl, 7                       
23A8  77 05                ja 0x23af                       
23AA  80 f9 02             cmp cl, 2                       
23AD  75 04                jne 0x23b3                      
23AF  e6 a0                out 0xa0, al                    
23B1  eb 00                jmp 0x23b3                      
23B3  e6 20                out 0x20, al                    
23B5  59                   pop cx                          
23B6  c3                   ret                             
23B7  06                   push es                         
23B8  b8 40 00             mov ax, 0x40                    
23BB  8e c0                mov es, ax                      
23BD  8b d1                mov dx, cx                      
23BF  33 c9                xor cx, cx                      
23C1  26 03 16 6c 00       add dx, word ptr es:[0x6c]      
23C6  26 13 0e 6e 00       adc cx, word ptr es:[0x6e]      
23CB  07                   pop es                          
23CC  fb                   sti                             
23CD  c3                   ret                             
23CE  06                   push es                         
23CF  b8 40 00             mov ax, 0x40                    
23D2  8e c0                mov es, ax                      
23D4  26 3b 0e 6e 00       cmp cx, word ptr es:[0x6e]      
23D9  75 09                jne 0x23e4                      
23DB  26 3b 16 6c 00       cmp dx, word ptr es:[0x6c]      
23E0  77 1c                ja 0x23fe                       
23E2  eb 1d                jmp 0x2401                      
23E4  83 f9 18             cmp cx, 0x18                    
23E7  72 0d                jb 0x23f6                       
23E9  26 a1 6e 00          mov ax, word ptr es:[0x6e]      
23ED  0b c0                or ax, ax                       
23EF  75 09                jne 0x23fa                      
23F1  83 c0 18             add ax, 0x18                    
23F4  eb 04                jmp 0x23fa                      
23F6  26 a1 6e 00          mov ax, word ptr es:[0x6e]      
23FA  3b c1                cmp ax, cx                      
23FC  77 03                ja 0x2401                       
23FE  f8                   clc                             
23FF  eb 01                jmp 0x2402                      
2401  f9                   stc                             
2402  07                   pop es                          
2403  c3                   ret                             
2404  1e                   push ds                         
2405  56                   push si                         
2406  51                   push cx                         
2407  50                   push ax                         
2408  8a e0                mov ah, al                      
240A  d1 e9                shr cx, 1                       
240C  89 04                mov word ptr [si], ax           
240E  83 c6 02             add si, 2                       
2411  e2 f9                loop 0x240c                     
2413  58                   pop ax                          
2414  59                   pop cx                          
2415  5e                   pop si                          
2416  1f                   pop ds                          
2417  c3                   ret                             
2418  06                   push es                         
2419  56                   push si                         
241A  57                   push di                         
241B  51                   push cx                         
241C  e8 19 00             call 0x2438                     
241F  0b c9                or cx, cx                       
2421  74 05                je 0x2428                       
2423  1e                   push ds                         
2424  07                   pop es                          
2425  fc                   cld                             
2426  f3 a4                rep movsb byte ptr es:[di], byte ptr [si]
2428  26 c6 05 00          mov byte ptr es:[di], 0         
242C  59                   pop cx                          
242D  5f                   pop di                          
242E  5e                   pop si                          
242F  07                   pop es                          
2430  c3                   ret                             
2431  56                   push si                         
2432  57                   push di                         
2433  f3 a6                repe cmpsb byte ptr [si], byte ptr es:[di]
2435  5f                   pop di                          
2436  5e                   pop si                          
2437  c3                   ret                             
2438  56                   push si                         
2439  33 c9                xor cx, cx                      
243B  fc                   cld                             
243C  8a 04                mov al, byte ptr [si]           
243E  0a c0                or al, al                       
2440  75 02                jne 0x2444                      
2442  eb 04                jmp 0x2448                      
2444  41                   inc cx                          
2445  46                   inc si                          
2446  eb f4                jmp 0x243c                      
2448  5e                   pop si                          
2449  c3                   ret                             
244A  c3                   ret                             
244B  52                   push dx                         
244C  53                   push bx                         
244D  9c                   pushf                           
244E  9c                   pushf                           
244F  fa                   cli                             
2450  33 db                xor bx, bx                      
2452  8b 1e d9 0b          mov bx, word ptr [0xbd9]        
2456  d1 e3                shl bx, 1                       
2458  d1 e3                shl bx, 1                       
245A  d1 e3                shl bx, 1                       
245C  8a c2                mov al, dl                      
245E  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
2462  2e ff 97 9d 4e       call word ptr cs:[bx + 0x4e9d]  
2467  5b                   pop bx                          
2468  f6 c7 02             test bh, 2                      
246B  74 01                je 0x246e                       
246D  fb                   sti                             
246E  9d                   popf                            
246F  5b                   pop bx                          
2470  5a                   pop dx                          
2471  c3                   ret                             
2472  52                   push dx                         
2473  53                   push bx                         
2474  9c                   pushf                           
2475  9c                   pushf                           
2476  fa                   cli                             
2477  33 db                xor bx, bx                      
2479  8b 1e db 0b          mov bx, word ptr [0xbdb]        
247D  d1 e3                shl bx, 1                       
247F  d1 e3                shl bx, 1                       
2481  d1 e3                shl bx, 1                       
2483  8a e0                mov ah, al                      
2485  86 c2                xchg dl, al                     
2487  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
248B  2e ff 97 15 4f       call word ptr cs:[bx + 0x4f15]  
2490  5b                   pop bx                          
2491  f6 c7 02             test bh, 2                      
2494  74 01                je 0x2497                       
2496  fb                   sti                             
2497  9d                   popf                            
2498  5b                   pop bx                          
2499  5a                   pop dx                          
249A  c3                   ret                             
249B  51                   push cx                         
249C  53                   push bx                         
249D  33 db                xor bx, bx                      
249F  8b 1e d9 0b          mov bx, word ptr [0xbd9]        
24A3  d1 e3                shl bx, 1                       
24A5  d1 e3                shl bx, 1                       
24A7  d1 e3                shl bx, 1                       
24A9  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
24AD  2e ff 97 a1 4e       call word ptr cs:[bx + 0x4ea1]  
24B2  5b                   pop bx                          
24B3  59                   pop cx                          
24B4  c3                   ret                             
24B5  52                   push dx                         
24B6  51                   push cx                         
24B7  53                   push bx                         
24B8  33 db                xor bx, bx                      
24BA  8b 1e db 0b          mov bx, word ptr [0xbdb]        
24BE  d1 e3                shl bx, 1                       
24C0  d1 e3                shl bx, 1                       
24C2  d1 e3                shl bx, 1                       
24C4  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
24C8  2e ff 97 19 4f       call word ptr cs:[bx + 0x4f19]  
24CD  5b                   pop bx                          
24CE  59                   pop cx                          
24CF  5a                   pop dx                          
24D0  c3                   ret                             
24D1  52                   push dx                         
24D2  50                   push ax                         
24D3  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
24D7  b0 00                mov al, 0                       
24D9  e8 03 00             call 0x24df                     
24DC  58                   pop ax                          
24DD  5a                   pop dx                          
24DE  c3                   ret                             
24DF  ee                   out dx, al                      
24E0  ee                   out dx, al                      
24E1  83 c2 02             add dx, 2                       
24E4  b0 01                mov al, 1                       
24E6  ee                   out dx, al                      
24E7  ee                   out dx, al                      
24E8  80 3e 61 0c 00       cmp byte ptr [0xc61], 0         
24ED  74 07                je 0x24f6                       
24EF  80 3e 15 0c 00       cmp byte ptr [0xc15], 0         
24F4  74 04                je 0x24fa                       
24F6  ee                   out dx, al                      
24F7  ee                   out dx, al                      
24F8  ee                   out dx, al                      
24F9  ee                   out dx, al                      
24FA  b0 04                mov al, 4                       
24FC  ee                   out dx, al                      
24FD  ee                   out dx, al                      
24FE  ee                   out dx, al                      
24FF  ee                   out dx, al                      
2500  83 ea 02             sub dx, 2                       
2503  c3                   ret                             
2504  50                   push ax                         
2505  52                   push dx                         
2506  a3 e6 0b             mov word ptr [0xbe6], ax        
2509  c6 06 e9 0b 01       mov byte ptr [0xbe9], 1         
250E  ba 12 00             mov dx, 0x12                    
2511  e8 37 ff             call 0x244b                     
2514  24 eb                and al, 0xeb                    
2516  f7 06 e6 0b 01 00    test word ptr [0xbe6], 1        
251C  74 02                je 0x2520                       
251E  0c 10                or al, 0x10                     
2520  f7 06 e6 0b 02 00    test word ptr [0xbe6], 2        
2526  74 02                je 0x252a                       
2528  0c 04                or al, 4                        
252A  24 f7                and al, 0xf7                    
252C  80 3e f4 0b ff       cmp byte ptr [0xbf4], 0xff      
2531  75 03                jne 0x2536                      
2533  a2 f4 0b             mov byte ptr [0xbf4], al        
2536  80 3e 17 0c 01       cmp byte ptr [0xc17], 1         
253B  75 02                jne 0x253f                      
253D  0c 80                or al, 0x80                     
253F  e8 30 ff             call 0x2472                     
2542  ba 0d 00             mov dx, 0xd                        ; 000D='IMGR'
2545  e8 03 ff             call 0x244b                     
2548  80 3e 61 0c 01       cmp byte ptr [0xc61], 1         
254D  74 04                je 0x2553                       
254F  24 20                and al, 0x20                    
2551  0c 10                or al, 0x10                     
2553  24 bf                and al, 0xbf                    
2555  f7 06 e6 0b 04 00    test word ptr [0xbe6], 4        
255B  75 02                jne 0x255f                      
255D  0c 02                or al, 2                        
255F  24 fb                and al, 0xfb                    
2561  50                   push ax                         
2562  e8 0d ff             call 0x2472                     
2565  58                   pop ax                          
2566  f7 06 e6 0b 04 00    test word ptr [0xbe6], 4        
256C  75 05                jne 0x2573                      
256E  0c 08                or al, 8                        
2570  e8 ff fe             call 0x2472                     
2573  f7 06 e6 0b 18 00    test word ptr [0xbe6], 0x18     
2579  74 1f                je 0x259a                       
257B  f7 06 e6 0b 08 00    test word ptr [0xbe6], 8        
2581  74 05                je 0x2588                       
2583  ba 09 00             mov dx, 9                       
2586  b0 40                mov al, 0x40                    
2588  f7 06 e6 0b 10 00    test word ptr [0xbe6], 0x10     
258E  74 02                je 0x2592                       
2590  0c 80                or al, 0x80                     
2592  e8 dd fe             call 0x2472                     
2595  c6 06 16 0c 01       mov byte ptr [0xc16], 1         
259A  80 3e 00 0c 01       cmp byte ptr [0xc00], 1         
259F  74 47                je 0x25e8                       
25A1  9c                   pushf                           
25A2  fa                   cli                             
25A3  52                   push dx                         
25A4  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
25A8  80 3e d7 0b 0c       cmp byte ptr [0xbd7], 0xc          ; 000C='SIMGR'
25AD  74 10                je 0x25bf                       
25AF  e8 b4 2e             call 0x5466                     
25B2  3c 01                cmp al, 1                       
25B4  75 0b                jne 0x25c1                      
25B6  83 c2 02             add dx, 2                       
25B9  ec                   in al, dx                       
25BA  24 1f                and al, 0x1f                    
25BC  0c 10                or al, 0x10                     
25BE  ee                   out dx, al                      
25BF  eb 1f                jmp 0x25e0                      
25C1  b0 01                mov al, 1                       
25C3  ee                   out dx, al                      
25C4  ee                   out dx, al                      
25C5  83 c2 02             add dx, 2                       
25C8  b0 01                mov al, 1                       
25CA  0c 10                or al, 0x10                     
25CC  ee                   out dx, al                      
25CD  ee                   out dx, al                      
25CE  ee                   out dx, al                      
25CF  ee                   out dx, al                      
25D0  ee                   out dx, al                      
25D1  ee                   out dx, al                      
25D2  ee                   out dx, al                      
25D3  ee                   out dx, al                      
25D4  b0 04                mov al, 4                       
25D6  0c 10                or al, 0x10                     
25D8  ee                   out dx, al                      
25D9  ee                   out dx, al                      
25DA  ee                   out dx, al                      
25DB  ee                   out dx, al                      
25DC  ee                   out dx, al                      
25DD  ee                   out dx, al                      
25DE  ee                   out dx, al                      
25DF  ee                   out dx, al                      
25E0  5a                   pop dx                          
25E1  58                   pop ax                          
25E2  f6 c4 02             test ah, 2                      
25E5  74 01                je 0x25e8                       
25E7  fb                   sti                             
25E8  5a                   pop dx                          
25E9  58                   pop ax                          
25EA  c3                   ret                             
25EB  53                   push bx                         
25EC  52                   push dx                         
25ED  8b d8                mov bx, ax                      
25EF  f7 c3 02 00          test bx, 2                      
25F3  74 17                je 0x260c                       
25F5  ba 12 00             mov dx, 0x12                    
25F8  e8 50 fe             call 0x244b                     
25FB  0c 08                or al, 8                        
25FD  ba 12 00             mov dx, 0x12                    
2600  80 3e 17 0c 01       cmp byte ptr [0xc17], 1         
2605  75 02                jne 0x2609                      
2607  0c 80                or al, 0x80                     
2609  e8 66 fe             call 0x2472                     
260C  ba 0d 00             mov dx, 0xd                        ; 000D='IMGR'
260F  e8 39 fe             call 0x244b                     
2612  24 fc                and al, 0xfc                    
2614  f7 c3 1b 00          test bx, 0x1b                   
2618  74 02                je 0x261c                       
261A  0c 01                or al, 1                        
261C  f7 c3 04 00          test bx, 4                      
2620  74 02                je 0x2624                       
2622  0c 02                or al, 2                        
2624  50                   push ax                         
2625  24 03                and al, 3                       
2627  0a c0                or al, al                       
2629  58                   pop ax                          
262A  74 06                je 0x2632                       
262C  ba 0d 00             mov dx, 0xd                        ; 000D='IMGR'
262F  e8 40 fe             call 0x2472                     
2632  5a                   pop dx                          
2633  5b                   pop bx                          
2634  c3                   ret                             
2635  52                   push dx                         
2636  ba 08 00             mov dx, 8                       
2639  e8 0f fe             call 0x244b                     
263C  24 04                and al, 4                       
263E  5a                   pop dx                          
263F  c3                   ret                             
2640  c6 06 e9 0b 00       mov byte ptr [0xbe9], 0         
2645  9c                   pushf                           
2646  fa                   cli                             
2647  52                   push dx                         
2648  e8 1b 2e             call 0x5466                     
264B  3c 00                cmp al, 0                       
264D  75 17                jne 0x2666                      
264F  80 3e d7 0b 0c       cmp byte ptr [0xbd7], 0xc          ; 000C='SIMGR'
2654  74 10                je 0x2666                       
2656  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
265A  83 c2 02             add dx, 2                       
265D  b0 04                mov al, 4                       
265F  ee                   out dx, al                      
2660  83 ea 02             sub dx, 2                       
2663  e8 6b fe             call 0x24d1                     
2666  ba 0d 00             mov dx, 0xd                        ; 000D='IMGR'
2669  e8 df fd             call 0x244b                     
266C  24 20                and al, 0x20                    
266E  0c 03                or al, 3                        
2670  e8 ff fd             call 0x2472                     
2673  ba 12 00             mov dx, 0x12                    
2676  e8 d2 fd             call 0x244b                     
2679  24 e3                and al, 0xe3                    
267B  ba 12 00             mov dx, 0x12                    
267E  c6 06 f4 0b ff       mov byte ptr [0xbf4], 0xff      
2683  80 3e 17 0c 01       cmp byte ptr [0xc17], 1         
2688  75 02                jne 0x268c                      
268A  0c 80                or al, 0x80                     
268C  e8 e3 fd             call 0x2472                     
268F  80 3e 16 0c 00       cmp byte ptr [0xc16], 0         
2694  74 0d                je 0x26a3                       
2696  33 c0                xor ax, ax                      
2698  ba 09 00             mov dx, 9                       
269B  e8 d4 fd             call 0x2472                     
269E  c6 06 16 0c 00       mov byte ptr [0xc16], 0         
26A3  5a                   pop dx                          
26A4  58                   pop ax                          
26A5  f6 c4 02             test ah, 2                      
26A8  74 01                je 0x26ab                       
26AA  fb                   sti                             
26AB  c3                   ret                             
26AC  51                   push cx                         
26AD  52                   push dx                         
26AE  ba 0c 00             mov dx, 0xc                        ; 000C='SIMGR'
26B1  e8 97 fd             call 0x244b                     
26B4  ba 0c 00             mov dx, 0xc                        ; 000C='SIMGR'
26B7  0c 02                or al, 2                        
26B9  e8 b6 fd             call 0x2472                     
26BC  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
26C0  b9 00 01             mov cx, 0x100                   
26C3  ec                   in al, dx                       
26C4  ec                   in al, dx                       
26C5  49                   dec cx                          
26C6  75 fb                jne 0x26c3                      
26C8  ba 0c 00             mov dx, 0xc                        ; 000C='SIMGR'
26CB  e8 7d fd             call 0x244b                     
26CE  ba 0c 00             mov dx, 0xc                        ; 000C='SIMGR'
26D1  24 fd                and al, 0xfd                    
26D3  e8 9c fd             call 0x2472                     
26D6  5a                   pop dx                          
26D7  59                   pop cx                          
26D8  c3                   ret                             
26D9  51                   push cx                         
26DA  50                   push ax                         
26DB  9c                   pushf                           
26DC  fa                   cli                             
26DD  80 3e 00 0c 01       cmp byte ptr [0xc00], 1         
26E2  74 03                je 0x26e7                       
26E4  e9 0f 01             jmp 0x27f6                      
26E7  a2 02 0c             mov byte ptr [0xc02], al        
26EA  80 3e 15 0c 00       cmp byte ptr [0xc15], 0         
26EF  74 06                je 0x26f7                       
26F1  e8 0c 01             call 0x2800                     
26F4  e9 ff 00             jmp 0x27f6                      
26F7  53                   push bx                         
26F8  52                   push dx                         
26F9  50                   push ax                         
26FA  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
26FE  83 c2 02             add dx, 2                       
2701  ec                   in al, dx                       
2702  24 1f                and al, 0x1f                    
2704  a2 03 0c             mov byte ptr [0xc03], al        
2707  a0 02 0c             mov al, byte ptr [0xc02]        
270A  24 f8                and al, 0xf8                    
270C  3c e0                cmp al, 0xe0                    
270E  74 0d                je 0x271d                       
2710  3c 20                cmp al, 0x20                    
2712  74 09                je 0x271d                       
2714  3c d0                cmp al, 0xd0                    
2716  74 05                je 0x271d                       
2718  b0 04                mov al, 4                       
271A  ee                   out dx, al                      
271B  eb 06                jmp 0x2723                      
271D  a0 03 0c             mov al, byte ptr [0xc03]        
2720  24 0f                and al, 0xf                     
2722  ee                   out dx, al                      
2723  eb 00                jmp 0x2725                      
2725  83 ea 02             sub dx, 2                       
2728  b0 22                mov al, 0x22                    
272A  ee                   out dx, al                      
272B  ee                   out dx, al                      
272C  eb 00                jmp 0x272e                      
272E  b0 aa                mov al, 0xaa                    
2730  ee                   out dx, al                      
2731  ee                   out dx, al                      
2732  eb 00                jmp 0x2734                      
2734  b0 55                mov al, 0x55                    
2736  ee                   out dx, al                      
2737  ee                   out dx, al                      
2738  eb 00                jmp 0x273a                      
273A  b0 00                mov al, 0                       
273C  ee                   out dx, al                      
273D  ee                   out dx, al                      
273E  eb 00                jmp 0x2740                      
2740  b0 ff                mov al, 0xff                    
2742  ee                   out dx, al                      
2743  ee                   out dx, al                      
2744  eb 00                jmp 0x2746                      
2746  b0 87                mov al, 0x87                    
2748  ee                   out dx, al                      
2749  ee                   out dx, al                      
274A  eb 00                jmp 0x274c                      
274C  b0 78                mov al, 0x78                    
274E  ee                   out dx, al                      
274F  ee                   out dx, al                      
2750  eb 00                jmp 0x2752                      
2752  58                   pop ax                          
2753  50                   push ax                         
2754  ee                   out dx, al                      
2755  ee                   out dx, al                      
2756  eb 00                jmp 0x2758                      
2758  50                   push ax                         
2759  83 c2 02             add dx, 2                       
275C  b0 04                mov al, 4                       
275E  ee                   out dx, al                      
275F  ee                   out dx, al                      
2760  eb 00                jmp 0x2762                      
2762  83 ea 02             sub dx, 2                       
2765  58                   pop ax                          
2766  50                   push ax                         
2767  24 f8                and al, 0xf8                    
2769  3c 10                cmp al, 0x10                    
276B  58                   pop ax                          
276C  75 10                jne 0x277e                      
276E  e8 e6 01             call 0x2957                     
2771  8a fc                mov bh, ah                      
2773  e8 e1 01             call 0x2957                     
2776  8a dc                mov bl, ah                      
2778  89 1e be 0b          mov word ptr [0xbbe], bx        
277C  eb 48                jmp 0x27c6                      
277E  50                   push ax                         
277F  24 f8                and al, 0xf8                    
2781  3c 08                cmp al, 8                       
2783  58                   pop ax                          
2784  75 08                jne 0x278e                      
2786  42                   inc dx                          
2787  ec                   in al, dx                       
2788  a2 bd 0b             mov byte ptr [0xbbd], al        
278B  4a                   dec dx                          
278C  eb 38                jmp 0x27c6                      
278E  3c 00                cmp al, 0                       
2790  75 21                jne 0x27b3                      
2792  50                   push ax                         
2793  83 c2 02             add dx, 2                       
2796  ec                   in al, dx                       
2797  24 10                and al, 0x10                    
2799  0c 05                or al, 5                        
279B  ee                   out dx, al                      
279C  ee                   out dx, al                      
279D  eb 00                jmp 0x279f                      
279F  24 fe                and al, 0xfe                    
27A1  ee                   out dx, al                      
27A2  ee                   out dx, al                      
27A3  83 ea 02             sub dx, 2                       
27A6  58                   pop ax                          
27A7  fe c0                inc al                          
27A9  3c 08                cmp al, 8                       
27AB  74 19                je 0x27c6                       
27AD  ee                   out dx, al                      
27AE  ee                   out dx, al                      
27AF  eb 00                jmp 0x27b1                      
27B1  eb df                jmp 0x2792                      
27B3  83 c2 02             add dx, 2                       
27B6  ec                   in al, dx                       
27B7  24 10                and al, 0x10                    
27B9  0c 05                or al, 5                        
27BB  ee                   out dx, al                      
27BC  ee                   out dx, al                      
27BD  eb 00                jmp 0x27bf                      
27BF  24 fe                and al, 0xfe                    
27C1  ee                   out dx, al                      
27C2  ee                   out dx, al                      
27C3  83 ea 02             sub dx, 2                       
27C6  58                   pop ax                          
27C7  a0 02 0c             mov al, byte ptr [0xc02]        
27CA  24 f8                and al, 0xf8                    
27CC  3c 48                cmp al, 0x48                    
27CE  75 0e                jne 0x27de                      
27D0  83 c2 02             add dx, 2                       
27D3  a0 03 0c             mov al, byte ptr [0xc03]        
27D6  24 0f                and al, 0xf                     
27D8  0c 10                or al, 0x10                     
27DA  ee                   out dx, al                      
27DB  83 ea 02             sub dx, 2                       
27DE  3c 30                cmp al, 0x30                    
27E0  75 0c                jne 0x27ee                      
27E2  83 c2 02             add dx, 2                       
27E5  a0 12 0c             mov al, byte ptr [0xc12]        
27E8  24 0f                and al, 0xf                     
27EA  ee                   out dx, al                      
27EB  83 ea 02             sub dx, 2                       
27EE  eb 00                jmp 0x27f0                      
27F0  b0 ff                mov al, 0xff                    
27F2  ee                   out dx, al                      
27F3  ee                   out dx, al                      
27F4  5a                   pop dx                          
27F5  5b                   pop bx                          
27F6  58                   pop ax                          
27F7  f6 c4 02             test ah, 2                      
27FA  74 01                je 0x27fd                       
27FC  fb                   sti                             
27FD  58                   pop ax                          
27FE  59                   pop cx                          
27FF  c3                   ret                             
2800  53                   push bx                         
2801  52                   push dx                         
2802  50                   push ax                         
2803  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
2807  83 c2 02             add dx, 2                       
280A  ec                   in al, dx                       
280B  24 1f                and al, 0x1f                    
280D  a2 03 0c             mov byte ptr [0xc03], al        
2810  a0 02 0c             mov al, byte ptr [0xc02]        
2813  24 f8                and al, 0xf8                    
2815  3c e0                cmp al, 0xe0                    
2817  74 0d                je 0x2826                       
2819  3c 20                cmp al, 0x20                    
281B  74 09                je 0x2826                       
281D  3c d0                cmp al, 0xd0                    
281F  74 05                je 0x2826                       
2821  b0 04                mov al, 4                       
2823  ee                   out dx, al                      
2824  eb 06                jmp 0x282c                      
2826  a0 03 0c             mov al, byte ptr [0xc03]        
2829  24 0f                and al, 0xf                     
282B  ee                   out dx, al                      
282C  eb 00                jmp 0x282e                      
282E  83 ea 02             sub dx, 2                       
2831  b0 22                mov al, 0x22                    
2833  ee                   out dx, al                      
2834  ee                   out dx, al                      
2835  ee                   out dx, al                      
2836  ee                   out dx, al                      
2837  ee                   out dx, al                      
2838  ee                   out dx, al                      
2839  ee                   out dx, al                      
283A  ee                   out dx, al                      
283B  eb 00                jmp 0x283d                      
283D  b0 aa                mov al, 0xaa                    
283F  ee                   out dx, al                      
2840  ee                   out dx, al                      
2841  ee                   out dx, al                      
2842  ee                   out dx, al                      
2843  ee                   out dx, al                      
2844  ee                   out dx, al                      
2845  ee                   out dx, al                      
2846  ee                   out dx, al                      
2847  eb 00                jmp 0x2849                      
2849  b0 55                mov al, 0x55                    
284B  ee                   out dx, al                      
284C  ee                   out dx, al                      
284D  ee                   out dx, al                      
284E  ee                   out dx, al                      
284F  ee                   out dx, al                      
2850  ee                   out dx, al                      
2851  ee                   out dx, al                      
2852  ee                   out dx, al                      
2853  eb 00                jmp 0x2855                      
2855  b0 00                mov al, 0                       
2857  ee                   out dx, al                      
2858  ee                   out dx, al                      
2859  ee                   out dx, al                      
285A  ee                   out dx, al                      
285B  ee                   out dx, al                      
285C  ee                   out dx, al                      
285D  ee                   out dx, al                      
285E  ee                   out dx, al                      
285F  eb 00                jmp 0x2861                      
2861  b0 ff                mov al, 0xff                    
2863  ee                   out dx, al                      
2864  ee                   out dx, al                      
2865  ee                   out dx, al                      
2866  ee                   out dx, al                      
2867  ee                   out dx, al                      
2868  ee                   out dx, al                      
2869  ee                   out dx, al                      
286A  ee                   out dx, al                      
286B  eb 00                jmp 0x286d                      
286D  b0 87                mov al, 0x87                    
286F  ee                   out dx, al                      
2870  ee                   out dx, al                      
2871  ee                   out dx, al                      
2872  ee                   out dx, al                      
2873  ee                   out dx, al                      
2874  ee                   out dx, al                      
2875  ee                   out dx, al                      
2876  ee                   out dx, al                      
2877  eb 00                jmp 0x2879                      
2879  b0 78                mov al, 0x78                    
287B  ee                   out dx, al                      
287C  ee                   out dx, al                      
287D  ee                   out dx, al                      
287E  ee                   out dx, al                      
287F  ee                   out dx, al                      
2880  ee                   out dx, al                      
2881  ee                   out dx, al                      
2882  ee                   out dx, al                      
2883  eb 00                jmp 0x2885                      
2885  58                   pop ax                          
2886  50                   push ax                         
2887  ee                   out dx, al                      
2888  ee                   out dx, al                      
2889  ee                   out dx, al                      
288A  ee                   out dx, al                      
288B  ee                   out dx, al                      
288C  ee                   out dx, al                      
288D  ee                   out dx, al                      
288E  ee                   out dx, al                      
288F  eb 00                jmp 0x2891                      
2891  50                   push ax                         
2892  83 c2 02             add dx, 2                       
2895  b0 04                mov al, 4                       
2897  ee                   out dx, al                      
2898  83 ea 02             sub dx, 2                       
289B  58                   pop ax                          
289C  50                   push ax                         
289D  24 f8                and al, 0xf8                    
289F  3c 10                cmp al, 0x10                    
28A1  58                   pop ax                          
28A2  75 10                jne 0x28b4                      
28A4  e8 b0 00             call 0x2957                     
28A7  8a fc                mov bh, ah                      
28A9  e8 ab 00             call 0x2957                     
28AC  8a dc                mov bl, ah                      
28AE  89 1e be 0b          mov word ptr [0xbbe], bx        
28B2  eb 6a                jmp 0x291e                      
28B4  50                   push ax                         
28B5  24 f8                and al, 0xf8                    
28B7  3c 08                cmp al, 8                       
28B9  58                   pop ax                          
28BA  75 08                jne 0x28c4                      
28BC  42                   inc dx                          
28BD  ec                   in al, dx                       
28BE  a2 bd 0b             mov byte ptr [0xbbd], al        
28C1  4a                   dec dx                          
28C2  eb 5a                jmp 0x291e                      
28C4  3c 00                cmp al, 0                       
28C6  75 35                jne 0x28fd                      
28C8  50                   push ax                         
28C9  83 c2 02             add dx, 2                       
28CC  ec                   in al, dx                       
28CD  24 10                and al, 0x10                    
28CF  0c 05                or al, 5                        
28D1  ee                   out dx, al                      
28D2  ee                   out dx, al                      
28D3  ee                   out dx, al                      
28D4  ee                   out dx, al                      
28D5  ee                   out dx, al                      
28D6  ee                   out dx, al                      
28D7  ee                   out dx, al                      
28D8  ee                   out dx, al                      
28D9  eb 00                jmp 0x28db                      
28DB  24 fe                and al, 0xfe                    
28DD  ee                   out dx, al                      
28DE  ee                   out dx, al                      
28DF  ee                   out dx, al                      
28E0  ee                   out dx, al                      
28E1  ee                   out dx, al                      
28E2  ee                   out dx, al                      
28E3  ee                   out dx, al                      
28E4  ee                   out dx, al                      
28E5  eb 00                jmp 0x28e7                      
28E7  83 ea 02             sub dx, 2                       
28EA  58                   pop ax                          
28EB  fe c0                inc al                          
28ED  3c 08                cmp al, 8                       
28EF  74 2d                je 0x291e                       
28F1  ee                   out dx, al                      
28F2  ee                   out dx, al                      
28F3  ee                   out dx, al                      
28F4  ee                   out dx, al                      
28F5  ee                   out dx, al                      
28F6  ee                   out dx, al                      
28F7  ee                   out dx, al                      
28F8  ee                   out dx, al                      
28F9  eb 00                jmp 0x28fb                      
28FB  eb cb                jmp 0x28c8                      
28FD  83 c2 02             add dx, 2                       
2900  ec                   in al, dx                       
2901  24 10                and al, 0x10                    
2903  0c 05                or al, 5                        
2905  ee                   out dx, al                      
2906  ee                   out dx, al                      
2907  ee                   out dx, al                      
2908  ee                   out dx, al                      
2909  ee                   out dx, al                      
290A  ee                   out dx, al                      
290B  ee                   out dx, al                      
290C  ee                   out dx, al                      
290D  eb 00                jmp 0x290f                      
290F  24 fe                and al, 0xfe                    
2911  ee                   out dx, al                      
2912  ee                   out dx, al                      
2913  ee                   out dx, al                      
2914  ee                   out dx, al                      
2915  ee                   out dx, al                      
2916  ee                   out dx, al                      
2917  ee                   out dx, al                      
2918  ee                   out dx, al                      
2919  eb 00                jmp 0x291b                      
291B  83 ea 02             sub dx, 2                       
291E  58                   pop ax                          
291F  a0 02 0c             mov al, byte ptr [0xc02]        
2922  24 f8                and al, 0xf8                    
2924  3c 48                cmp al, 0x48                    
2926  75 0e                jne 0x2936                      
2928  83 c2 02             add dx, 2                       
292B  a0 03 0c             mov al, byte ptr [0xc03]        
292E  24 0f                and al, 0xf                     
2930  0c 10                or al, 0x10                     
2932  ee                   out dx, al                      
2933  83 ea 02             sub dx, 2                       
2936  3c 30                cmp al, 0x30                    
2938  75 0c                jne 0x2946                      
293A  83 c2 02             add dx, 2                       
293D  a0 12 0c             mov al, byte ptr [0xc12]        
2940  24 0f                and al, 0xf                     
2942  ee                   out dx, al                      
2943  83 ea 02             sub dx, 2                       
2946  eb 00                jmp 0x2948                      
2948  b0 ff                mov al, 0xff                    
294A  ee                   out dx, al                      
294B  ee                   out dx, al                      
294C  ee                   out dx, al                      
294D  ee                   out dx, al                      
294E  ee                   out dx, al                      
294F  ee                   out dx, al                      
2950  ee                   out dx, al                      
2951  ee                   out dx, al                      
2952  eb 00                jmp 0x2954                      
2954  5a                   pop dx                          
2955  5b                   pop bx                          
2956  c3                   ret                             
2957  42                   inc dx                          
2958  ec                   in al, dx                       
2959  ec                   in al, dx                       
295A  8a e0                mov ah, al                      
295C  80 e4 f0             and ah, 0xf0                    
295F  42                   inc dx                          
2960  b0 05                mov al, 5                       
2962  ee                   out dx, al                      
2963  ee                   out dx, al                      
2964  ee                   out dx, al                      
2965  ee                   out dx, al                      
2966  ee                   out dx, al                      
2967  ee                   out dx, al                      
2968  b0 04                mov al, 4                       
296A  ee                   out dx, al                      
296B  ee                   out dx, al                      
296C  ee                   out dx, al                      
296D  ee                   out dx, al                      
296E  ee                   out dx, al                      
296F  ee                   out dx, al                      
2970  4a                   dec dx                          
2971  eb 00                jmp 0x2973                      
2973  ec                   in al, dx                       
2974  ec                   in al, dx                       
2975  d0 e8                shr al, 1                       
2977  d0 e8                shr al, 1                       
2979  d0 e8                shr al, 1                       
297B  d0 e8                shr al, 1                       
297D  0a e0                or ah, al                       
297F  42                   inc dx                          
2980  b0 05                mov al, 5                       
2982  ee                   out dx, al                      
2983  ee                   out dx, al                      
2984  ee                   out dx, al                      
2985  ee                   out dx, al                      
2986  ee                   out dx, al                      
2987  ee                   out dx, al                      
2988  b0 04                mov al, 4                       
298A  ee                   out dx, al                      
298B  ee                   out dx, al                      
298C  ee                   out dx, al                      
298D  ee                   out dx, al                      
298E  ee                   out dx, al                      
298F  ee                   out dx, al                      
2990  4a                   dec dx                          
2991  4a                   dec dx                          
2992  c3                   ret                             
2993  52                   push dx                         
2994  50                   push ax                         
2995  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
2999  83 c2 02             add dx, 2                       
299C  b0 0c                mov al, 0xc                        ; 000C='SIMGR'
299E  ee                   out dx, al                      
299F  eb 00                jmp 0x29a1                      
29A1  b0 04                mov al, 4                       
29A3  ee                   out dx, al                      
29A4  e8 2a fb             call 0x24d1                     
29A7  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
29AB  83 c2 02             add dx, 2                       
29AE  b0 0c                mov al, 0xc                        ; 000C='SIMGR'
29B0  ee                   out dx, al                      
29B1  80 3e 15 0c 00       cmp byte ptr [0xc15], 0         
29B6  74 03                je 0x29bb                       
29B8  ee                   out dx, al                      
29B9  ee                   out dx, al                      
29BA  ee                   out dx, al                      
29BB  83 ea 02             sub dx, 2                       
29BE  b0 10                mov al, 0x10                    
29C0  ee                   out dx, al                      
29C1  80 3e 15 0c 00       cmp byte ptr [0xc15], 0         
29C6  74 03                je 0x29cb                       
29C8  ee                   out dx, al                      
29C9  ee                   out dx, al                      
29CA  ee                   out dx, al                      
29CB  83 c2 02             add dx, 2                       
29CE  b0 06                mov al, 6                       
29D0  ee                   out dx, al                      
29D1  ee                   out dx, al                      
29D2  ee                   out dx, al                      
29D3  80 3e 15 0c 00       cmp byte ptr [0xc15], 0         
29D8  74 03                je 0x29dd                       
29DA  ee                   out dx, al                      
29DB  ee                   out dx, al                      
29DC  ee                   out dx, al                      
29DD  4a                   dec dx                          
29DE  51                   push cx                         
29DF  b9 00 01             mov cx, 0x100                   
29E2  ec                   in al, dx                       
29E3  a8 40                test al, 0x40                   
29E5  e0 fb                loopne 0x29e2                   
29E7  42                   inc dx                          
29E8  0b c9                or cx, cx                       
29EA  b0 0c                mov al, 0xc                        ; 000C='SIMGR'
29EC  59                   pop cx                          
29ED  74 25                je 0x2a14                       
29EF  b0 07                mov al, 7                       
29F1  ee                   out dx, al                      
29F2  ee                   out dx, al                      
29F3  ee                   out dx, al                      
29F4  80 3e 15 0c 00       cmp byte ptr [0xc15], 0         
29F9  74 03                je 0x29fe                       
29FB  ee                   out dx, al                      
29FC  ee                   out dx, al                      
29FD  ee                   out dx, al                      
29FE  b0 04                mov al, 4                       
2A00  ee                   out dx, al                      
2A01  ee                   out dx, al                      
2A02  ee                   out dx, al                      
2A03  80 3e 15 0c 00       cmp byte ptr [0xc15], 0         
2A08  74 03                je 0x2a0d                       
2A0A  ee                   out dx, al                      
2A0B  ee                   out dx, al                      
2A0C  ee                   out dx, al                      
2A0D  83 ea 02             sub dx, 2                       
2A10  f8                   clc                             
2A11  58                   pop ax                          
2A12  5a                   pop dx                          
2A13  c3                   ret                             
2A14  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
2A18  83 c2 02             add dx, 2                       
2A1B  b0 0c                mov al, 0xc                        ; 000C='SIMGR'
2A1D  ee                   out dx, al                      
2A1E  f9                   stc                             
2A1F  eb f0                jmp 0x2a11                      
2A21  e8 6f ff             call 0x2993                     
2A24  73 23                jae 0x2a49                      
2A26  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
2A2A  83 c2 02             add dx, 2                       
2A2D  b0 0c                mov al, 0xc                        ; 000C='SIMGR'
2A2F  ee                   out dx, al                      
2A30  0c 02                or al, 2                        
2A32  eb 00                jmp 0x2a34                      
2A34  ee                   out dx, al                      
2A35  4a                   dec dx                          
2A36  51                   push cx                         
2A37  b9 00 80             mov cx, 0x8000                  
2A3A  ec                   in al, dx                       
2A3B  a8 40                test al, 0x40                   
2A3D  e1 fb                loope 0x2a3a                    
2A3F  59                   pop cx                          
2A40  42                   inc dx                          
2A41  eb 00                jmp 0x2a43                      
2A43  b0 0c                mov al, 0xc                        ; 000C='SIMGR'
2A45  ee                   out dx, al                      
2A46  e8 4a ff             call 0x2993                     
2A49  c3                   ret                             
2A4A  52                   push dx                         
2A4B  50                   push ax                         
2A4C  80 3e 00 0c 01       cmp byte ptr [0xc00], 1         
2A51  75 00                jne 0x2a53                      
2A53  e8 7b fa             call 0x24d1                     
2A56  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
2A5A  83 c2 02             add dx, 2                       
2A5D  b0 0c                mov al, 0xc                        ; 000C='SIMGR'
2A5F  ee                   out dx, al                      
2A60  80 3e 15 0c 00       cmp byte ptr [0xc15], 0         
2A65  74 03                je 0x2a6a                       
2A67  ee                   out dx, al                      
2A68  ee                   out dx, al                      
2A69  ee                   out dx, al                      
2A6A  83 ea 02             sub dx, 2                       
2A6D  b0 40                mov al, 0x40                    
2A6F  ee                   out dx, al                      
2A70  80 3e 15 0c 00       cmp byte ptr [0xc15], 0         
2A75  74 03                je 0x2a7a                       
2A77  ee                   out dx, al                      
2A78  ee                   out dx, al                      
2A79  ee                   out dx, al                      
2A7A  83 c2 02             add dx, 2                       
2A7D  b0 06                mov al, 6                       
2A7F  ee                   out dx, al                      
2A80  ee                   out dx, al                      
2A81  ee                   out dx, al                      
2A82  80 3e 15 0c 00       cmp byte ptr [0xc15], 0         
2A87  74 03                je 0x2a8c                       
2A89  ee                   out dx, al                      
2A8A  ee                   out dx, al                      
2A8B  ee                   out dx, al                      
2A8C  4a                   dec dx                          
2A8D  51                   push cx                         
2A8E  b9 00 01             mov cx, 0x100                   
2A91  ec                   in al, dx                       
2A92  a8 40                test al, 0x40                   
2A94  e0 fb                loopne 0x2a91                   
2A96  42                   inc dx                          
2A97  0b c9                or cx, cx                       
2A99  b0 0c                mov al, 0xc                        ; 000C='SIMGR'
2A9B  59                   pop cx                          
2A9C  74 2e                je 0x2acc                       
2A9E  b0 07                mov al, 7                       
2AA0  ee                   out dx, al                      
2AA1  ee                   out dx, al                      
2AA2  ee                   out dx, al                      
2AA3  80 3e 15 0c 00       cmp byte ptr [0xc15], 0         
2AA8  74 03                je 0x2aad                       
2AAA  ee                   out dx, al                      
2AAB  ee                   out dx, al                      
2AAC  ee                   out dx, al                      
2AAD  b0 04                mov al, 4                       
2AAF  80 3e 00 0c 01       cmp byte ptr [0xc00], 1         
2AB4  74 09                je 0x2abf                       
2AB6  80 3e e9 0b 01       cmp byte ptr [0xbe9], 1         
2ABB  75 02                jne 0x2abf                      
2ABD  0c 10                or al, 0x10                     
2ABF  ee                   out dx, al                      
2AC0  ee                   out dx, al                      
2AC1  ee                   out dx, al                      
2AC2  80 3e 15 0c 00       cmp byte ptr [0xc15], 0         
2AC7  74 03                je 0x2acc                       
2AC9  ee                   out dx, al                      
2ACA  ee                   out dx, al                      
2ACB  ee                   out dx, al                      
2ACC  0c 0c                or al, 0xc                         ; 000C='SIMGR'
2ACE  ee                   out dx, al                      
2ACF  ee                   out dx, al                      
2AD0  ee                   out dx, al                      
2AD1  80 3e 15 0c 00       cmp byte ptr [0xc15], 0         
2AD6  74 03                je 0x2adb                       
2AD8  ee                   out dx, al                      
2AD9  ee                   out dx, al                      
2ADA  ee                   out dx, al                      
2ADB  24 f7                and al, 0xf7                    
2ADD  ee                   out dx, al                      
2ADE  ee                   out dx, al                      
2ADF  ee                   out dx, al                      
2AE0  80 3e 15 0c 00       cmp byte ptr [0xc15], 0         
2AE5  74 03                je 0x2aea                       
2AE7  ee                   out dx, al                      
2AE8  ee                   out dx, al                      
2AE9  ee                   out dx, al                      
2AEA  83 ea 02             sub dx, 2                       
2AED  58                   pop ax                          
2AEE  5a                   pop dx                          
2AEF  c3                   ret                             
2AF0  8a c2                mov al, dl                      
2AF2  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
2AF6  ee                   out dx, al                      
2AF7  ee                   out dx, al                      
2AF8  ee                   out dx, al                      
2AF9  83 c2 02             add dx, 2                       
2AFC  b0 01                mov al, 1                       
2AFE  ee                   out dx, al                      
2AFF  ee                   out dx, al                      
2B00  ee                   out dx, al                      
2B01  ee                   out dx, al                      
2B02  ee                   out dx, al                      
2B03  ee                   out dx, al                      
2B04  b0 03                mov al, 3                       
2B06  ee                   out dx, al                      
2B07  ee                   out dx, al                      
2B08  ee                   out dx, al                      
2B09  ee                   out dx, al                      
2B0A  ee                   out dx, al                      
2B0B  4a                   dec dx                          
2B0C  ec                   in al, dx                       
2B0D  ec                   in al, dx                       
2B0E  ec                   in al, dx                       
2B0F  8a e0                mov ah, al                      
2B11  42                   inc dx                          
2B12  b0 04                mov al, 4                       
2B14  ee                   out dx, al                      
2B15  ee                   out dx, al                      
2B16  ee                   out dx, al                      
2B17  ee                   out dx, al                      
2B18  4a                   dec dx                          
2B19  ec                   in al, dx                       
2B1A  ec                   in al, dx                       
2B1B  ec                   in al, dx                       
2B1C  25 f0 f0             and ax, 0xf0f0                  
2B1F  d0 ec                shr ah, 1                       
2B21  d0 ec                shr ah, 1                       
2B23  d0 ec                shr ah, 1                       
2B25  d0 ec                shr ah, 1                       
2B27  0a c4                or al, ah                       
2B29  c3                   ret                             
2B2A  8a e0                mov ah, al                      
2B2C  8a c2                mov al, dl                      
2B2E  0c 60                or al, 0x60                     
2B30  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
2B34  ee                   out dx, al                      
2B35  ee                   out dx, al                      
2B36  ee                   out dx, al                      
2B37  83 c2 02             add dx, 2                       
2B3A  b0 01                mov al, 1                       
2B3C  ee                   out dx, al                      
2B3D  ee                   out dx, al                      
2B3E  ee                   out dx, al                      
2B3F  ee                   out dx, al                      
2B40  ee                   out dx, al                      
2B41  ee                   out dx, al                      
2B42  83 ea 02             sub dx, 2                       
2B45  86 c4                xchg ah, al                     
2B47  ee                   out dx, al                      
2B48  ee                   out dx, al                      
2B49  ee                   out dx, al                      
2B4A  83 c2 02             add dx, 2                       
2B4D  b0 04                mov al, 4                       
2B4F  ee                   out dx, al                      
2B50  ee                   out dx, al                      
2B51  ee                   out dx, al                      
2B52  c3                   ret                             
2B53  52                   push dx                         
2B54  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
2B58  83 c2 02             add dx, 2                       
2B5B  ec                   in al, dx                       
2B5C  24 1f                and al, 0x1f                    
2B5E  a2 12 0c             mov byte ptr [0xc12], al        
2B61  80 3e 00 0c 01       cmp byte ptr [0xc00], 1         
2B66  75 05                jne 0x2b6d                      
2B68  e8 a3 00             call 0x2c0e                     
2B6B  eb 0d                jmp 0x2b7a                      
2B6D  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
2B71  b0 04                mov al, 4                       
2B73  83 c2 02             add dx, 2                       
2B76  ee                   out dx, al                      
2B77  e8 57 f9             call 0x24d1                     
2B7A  b8 00 00             mov ax, 0                       
2B7D  80 3e d7 0b 08       cmp byte ptr [0xbd7], 8         
2B82  72 2d                jb 0x2bb1                       
2B84  b8 00 01             mov ax, 0x100                   
2B87  80 3e d7 0b 0a       cmp byte ptr [0xbd7], 0xa          ; 000A='SCSIMGR'
2B8C  72 23                jb 0x2bb1                       
2B8E  80 3e d7 0b 0c       cmp byte ptr [0xbd7], 0xc          ; 000C='SIMGR'
2B93  74 1e                je 0x2bb3                       
2B95  80 3e dd 0b 0a       cmp byte ptr [0xbdd], 0xa          ; 000A='SCSIMGR'
2B9A  72 15                jb 0x2bb1                       
2B9C  80 3e dd 0b 0c       cmp byte ptr [0xbdd], 0xc          ; 000C='SIMGR'
2BA1  73 0e                jae 0x2bb1                      
2BA3  b8 01 00             mov ax, 1                       
2BA6  50                   push ax                         
2BA7  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
2BAB  e8 9c fe             call 0x2a4a                     
2BAE  58                   pop ax                          
2BAF  eb 10                jmp 0x2bc1                      
2BB1  eb 0e                jmp 0x2bc1                      
2BB3  b8 00 02             mov ax, 0x200                   
2BB6  50                   push ax                         
2BB7  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
2BBB  e8 63 fe             call 0x2a21                     
2BBE  58                   pop ax                          
2BBF  eb 00                jmp 0x2bc1                      
2BC1  5a                   pop dx                          
2BC2  c3                   ret                             
2BC3  80 3e 00 0c 01       cmp byte ptr [0xc00], 1         
2BC8  75 05                jne 0x2bcf                      
2BCA  e8 56 00             call 0x2c23                     
2BCD  eb 3e                jmp 0x2c0d                      
2BCF  80 3e d7 0b 0c       cmp byte ptr [0xbd7], 0xc          ; 000C='SIMGR'
2BD4  75 29                jne 0x2bff                      
2BD6  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
2BDA  83 c2 02             add dx, 2                       
2BDD  b0 04                mov al, 4                       
2BDF  ee                   out dx, al                      
2BE0  eb 00                jmp 0x2be2                      
2BE2  81 c2 00 04          add dx, 0x400                   
2BE6  b0 34                mov al, 0x34                    
2BE8  ee                   out dx, al                      
2BE9  eb 00                jmp 0x2beb                      
2BEB  81 ea 00 04          sub dx, 0x400                   
2BEF  b0 0c                mov al, 0xc                        ; 000C='SIMGR'
2BF1  ee                   out dx, al                      
2BF2  ee                   out dx, al                      
2BF3  eb 00                jmp 0x2bf5                      
2BF5  b0 0e                mov al, 0xe                     
2BF7  ee                   out dx, al                      
2BF8  ee                   out dx, al                      
2BF9  eb 00                jmp 0x2bfb                      
2BFB  b0 04                mov al, 4                       
2BFD  ee                   out dx, al                      
2BFE  ee                   out dx, al                      
2BFF  e8 cf f8             call 0x24d1                     
2C02  a0 12 0c             mov al, byte ptr [0xc12]        
2C05  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
2C09  83 c2 02             add dx, 2                       
2C0C  ee                   out dx, al                      
2C0D  c3                   ret                             
2C0E  80 3e 00 0c 01       cmp byte ptr [0xc00], 1         
2C13  73 01                jae 0x2c16                      
2C15  c3                   ret                             
2C16  b0 e0                mov al, 0xe0                    
2C18  0a 06 ca 0b          or al, byte ptr [0xbca]         
2C1C  e8 ba fa             call 0x26d9                     
2C1F  e8 af f8             call 0x24d1                     
2C22  c3                   ret                             
2C23  80 3e 00 0c 01       cmp byte ptr [0xc00], 1         
2C28  73 01                jae 0x2c2b                      
2C2A  c3                   ret                             
2C2B  80 3e e9 0b 01       cmp byte ptr [0xbe9], 1         
2C30  74 0a                je 0x2c3c                       
2C32  c6 06 13 0c 00       mov byte ptr [0xc13], 0         
2C37  b0 40                mov al, 0x40                    
2C39  e8 9d fa             call 0x26d9                     
2C3C  b0 30                mov al, 0x30                    
2C3E  e8 98 fa             call 0x26d9                     
2C41  c3                   ret                             
2C42  50                   push ax                         
2C43  51                   push cx                         
2C44  c6 06 15 0c 00       mov byte ptr [0xc15], 0         
2C49  c6 06 00 0c 00       mov byte ptr [0xc00], 0         
2C4E  c6 06 ca 0b ff       mov byte ptr [0xbca], 0xff      
2C53  c6 06 04 0c 00       mov byte ptr [0xc04], 0         
2C58  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
2C5C  81 c2 02 04          add dx, 0x402                   
2C60  ec                   in al, dx                       
2C61  eb 00                jmp 0x2c63                      
2C63  24 34                and al, 0x34                    
2C65  ee                   out dx, al                      
2C66  e8 52 01             call 0x2dbb                     
2C69  0b c0                or ax, ax                       
2C6B  74 39                je 0x2ca6                       
2C6D  52                   push dx                         
2C6E  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
2C72  83 c2 02             add dx, 2                       
2C75  b0 04                mov al, 4                       
2C77  ee                   out dx, al                      
2C78  eb 00                jmp 0x2c7a                      
2C7A  b0 0c                mov al, 0xc                        ; 000C='SIMGR'
2C7C  ee                   out dx, al                      
2C7D  eb 00                jmp 0x2c7f                      
2C7F  b0 0e                mov al, 0xe                     
2C81  ee                   out dx, al                      
2C82  ee                   out dx, al                      
2C83  ee                   out dx, al                      
2C84  eb 00                jmp 0x2c86                      
2C86  b0 04                mov al, 4                       
2C88  ee                   out dx, al                      
2C89  ee                   out dx, al                      
2C8A  5a                   pop dx                          
2C8B  e8 2d 01             call 0x2dbb                     
2C8E  0b c0                or ax, ax                       
2C90  74 14                je 0x2ca6                       
2C92  c6 06 15 0c 01       mov byte ptr [0xc15], 1         
2C97  e8 21 01             call 0x2dbb                     
2C9A  0b c0                or ax, ax                       
2C9C  74 08                je 0x2ca6                       
2C9E  c6 06 15 0c 01       mov byte ptr [0xc15], 1         
2CA3  e9 a8 00             jmp 0x2d4e                      
2CA6  80 3e 15 0c 01       cmp byte ptr [0xc15], 1         
2CAB  74 22                je 0x2ccf                       
2CAD  51                   push cx                         
2CAE  b9 32 00             mov cx, 0x32                    
2CB1  c6 06 15 0c 00       mov byte ptr [0xc15], 0         
2CB6  e8 02 01             call 0x2dbb                     
2CB9  c6 06 15 0c 01       mov byte ptr [0xc15], 1         
2CBE  0b c0                or ax, ax                       
2CC0  75 03                jne 0x2cc5                      
2CC2  49                   dec cx                          
2CC3  75 ec                jne 0x2cb1                      
2CC5  0b c9                or cx, cx                       
2CC7  59                   pop cx                          
2CC8  75 05                jne 0x2ccf                      
2CCA  c6 06 15 0c 00       mov byte ptr [0xc15], 0         
2CCF  c6 06 00 0c 01       mov byte ptr [0xc00], 1         
2CD4  c6 06 ca 0b 00       mov byte ptr [0xbca], 0         
2CD9  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
2CDD  83 c2 02             add dx, 2                       
2CE0  ec                   in al, dx                       
2CE1  24 1f                and al, 0x1f                    
2CE3  a2 12 0c             mov byte ptr [0xc12], al        
2CE6  b0 30                mov al, 0x30                    
2CE8  e8 ee f9             call 0x26d9                     
2CEB  b0 40                mov al, 0x40                    
2CED  e8 e9 f9             call 0x26d9                     
2CF0  b0 50                mov al, 0x50                    
2CF2  e8 e4 f9             call 0x26d9                     
2CF5  b0 00                mov al, 0                       
2CF7  e8 df f9             call 0x26d9                     
2CFA  b9 08 00             mov cx, 8                       
2CFD  b4 00                mov ah, 0                       
2CFF  80 3e 06 0c 01       cmp byte ptr [0xc06], 1         
2D04  74 05                je 0x2d0b                       
2D06  c6 06 ca 0b ff       mov byte ptr [0xbca], 0xff      
2D0B  b0 10                mov al, 0x10                    
2D0D  0a c4                or al, ah                       
2D0F  e8 c7 f9             call 0x26d9                     
2D12  83 3e be 0b aa       cmp word ptr [0xbbe], -0x56     
2D17  74 15                je 0x2d2e                       
2D19  fe c4                inc ah                          
2D1B  49                   dec cx                          
2D1C  75 ed                jne 0x2d0b                      
2D1E  b4 ff                mov ah, 0xff                    
2D20  80 3e ca 0b ff       cmp byte ptr [0xbca], 0xff      
2D25  75 05                jne 0x2d2c                      
2D27  c6 06 00 0c 00       mov byte ptr [0xc00], 0         
2D2C  eb 20                jmp 0x2d4e                      
2D2E  80 3e ca 0b ff       cmp byte ptr [0xbca], 0xff      
2D33  75 09                jne 0x2d3e                      
2D35  c6 06 13 0c 01       mov byte ptr [0xc13], 1         
2D3A  88 26 ca 0b          mov byte ptr [0xbca], ah        
2D3E  51                   push cx                         
2D3F  50                   push ax                         
2D40  8a cc                mov cl, ah                      
2D42  b4 01                mov ah, 1                       
2D44  d2 e4                shl ah, cl                      
2D46  08 26 04 0c          or byte ptr [0xc04], ah         
2D4A  58                   pop ax                          
2D4B  59                   pop cx                          
2D4C  eb cb                jmp 0x2d19                      
2D4E  58                   pop ax                          
2D4F  59                   pop cx                          
2D50  c3                   ret                             
2D51  53                   push bx                         
2D52  52                   push dx                         
2D53  a3 fa 0b             mov word ptr [0xbfa], ax        
2D56  a0 15 0c             mov al, byte ptr [0xc15]        
2D59  50                   push ax                         
2D5A  c6 06 15 0c 01       mov byte ptr [0xc15], 1         
2D5F  e8 59 00             call 0x2dbb                     
2D62  0b c0                or ax, ax                       
2D64  74 13                je 0x2d79                       
2D66  e8 91 73             call 0xa0fa                     
2D69  0b c0                or ax, ax                       
2D6B  74 0c                je 0x2d79                       
2D6D  ba 0b 00             mov dx, 0xb                        ; 000B='CSIMGR'
2D70  e8 7d fd             call 0x2af0                     
2D73  24 f8                and al, 0xf8                    
2D75  3c c0                cmp al, 0xc0                    
2D77  74 05                je 0x2d7e                       
2D79  bb 00 00             mov bx, 0                       
2D7C  eb 34                jmp 0x2db2                      
2D7E  ba 0d 00             mov dx, 0xd                        ; 000D='IMGR'
2D81  e8 6c fd             call 0x2af0                     
2D84  bb 00 00             mov bx, 0                       
2D87  a8 a0                test al, 0xa0                   
2D89  74 27                je 0x2db2                       
2D8B  24 df                and al, 0xdf                    
2D8D  a8 80                test al, 0x80                   
2D8F  74 02                je 0x2d93                       
2D91  0c 20                or al, 0x20                     
2D93  ba 0d 00             mov dx, 0xd                        ; 000D='IMGR'
2D96  e8 91 fd             call 0x2b2a                     
2D99  c6 06 00 0c 01       mov byte ptr [0xc00], 1         
2D9E  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
2DA2  83 c2 02             add dx, 2                       
2DA5  ec                   in al, dx                       
2DA6  24 1f                and al, 0x1f                    
2DA8  0c 0c                or al, 0xc                         ; 000C='SIMGR'
2DAA  a2 12 0c             mov byte ptr [0xc12], al        
2DAD  b0 30                mov al, 0x30                    
2DAF  e8 27 f9             call 0x26d9                     
2DB2  58                   pop ax                          
2DB3  a2 15 0c             mov byte ptr [0xc15], al        
2DB6  8b c3                mov ax, bx                      
2DB8  5a                   pop dx                          
2DB9  5b                   pop bx                          
2DBA  c3                   ret                             
2DBB  52                   push dx                         
2DBC  53                   push bx                         
2DBD  9c                   pushf                           
2DBE  fa                   cli                             
2DBF  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
2DC3  83 c2 02             add dx, 2                       
2DC6  ec                   in al, dx                       
2DC7  24 1f                and al, 0x1f                    
2DC9  8a d8                mov bl, al                      
2DCB  b0 04                mov al, 4                       
2DCD  ee                   out dx, al                      
2DCE  83 ea 02             sub dx, 2                       
2DD1  80 3e 15 0c 01       cmp byte ptr [0xc15], 1         
2DD6  75 05                jne 0x2ddd                      
2DD8  e8 72 00             call 0x2e4d                     
2DDB  eb 5a                jmp 0x2e37                      
2DDD  b0 22                mov al, 0x22                    
2DDF  ee                   out dx, al                      
2DE0  ee                   out dx, al                      
2DE1  eb 00                jmp 0x2de3                      
2DE3  b0 aa                mov al, 0xaa                    
2DE5  ee                   out dx, al                      
2DE6  ee                   out dx, al                      
2DE7  eb 00                jmp 0x2de9                      
2DE9  b0 55                mov al, 0x55                    
2DEB  ee                   out dx, al                      
2DEC  ee                   out dx, al                      
2DED  eb 00                jmp 0x2def                      
2DEF  b0 00                mov al, 0                       
2DF1  ee                   out dx, al                      
2DF2  ee                   out dx, al                      
2DF3  eb 00                jmp 0x2df5                      
2DF5  b0 ff                mov al, 0xff                    
2DF7  ee                   out dx, al                      
2DF8  ee                   out dx, al                      
2DF9  eb 00                jmp 0x2dfb                      
2DFB  42                   inc dx                          
2DFC  ec                   in al, dx                       
2DFD  24 f0                and al, 0xf0                    
2DFF  3c b0                cmp al, 0xb0                    
2E01  b8 ff ff             mov ax, 0xffff                  
2E04  75 31                jne 0x2e37                      
2E06  4a                   dec dx                          
2E07  b0 87                mov al, 0x87                    
2E09  ee                   out dx, al                      
2E0A  ee                   out dx, al                      
2E0B  42                   inc dx                          
2E0C  eb 00                jmp 0x2e0e                      
2E0E  ec                   in al, dx                       
2E0F  24 f0                and al, 0xf0                    
2E11  3c 50                cmp al, 0x50                    
2E13  b8 ff ff             mov ax, 0xffff                  
2E16  75 1f                jne 0x2e37                      
2E18  4a                   dec dx                          
2E19  b0 78                mov al, 0x78                    
2E1B  ee                   out dx, al                      
2E1C  ee                   out dx, al                      
2E1D  42                   inc dx                          
2E1E  eb 00                jmp 0x2e20                      
2E20  ec                   in al, dx                       
2E21  24 b0                and al, 0xb0                    
2E23  3c b0                cmp al, 0xb0                    
2E25  b8 ff ff             mov ax, 0xffff                  
2E28  75 0d                jne 0x2e37                      
2E2A  4a                   dec dx                          
2E2B  b0 08                mov al, 8                       
2E2D  ee                   out dx, al                      
2E2E  ee                   out dx, al                      
2E2F  b0 ff                mov al, 0xff                    
2E31  ee                   out dx, al                      
2E32  ee                   out dx, al                      
2E33  eb 00                jmp 0x2e35                      
2E35  33 c0                xor ax, ax                      
2E37  50                   push ax                         
2E38  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
2E3C  8a c3                mov al, bl                      
2E3E  83 c2 02             add dx, 2                       
2E41  ee                   out dx, al                      
2E42  58                   pop ax                          
2E43  5b                   pop bx                          
2E44  f6 c7 02             test bh, 2                      
2E47  74 01                je 0x2e4a                       
2E49  fb                   sti                             
2E4A  5b                   pop bx                          
2E4B  5a                   pop dx                          
2E4C  c3                   ret                             
2E4D  52                   push dx                         
2E4E  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
2E52  b0 22                mov al, 0x22                    
2E54  ee                   out dx, al                      
2E55  ee                   out dx, al                      
2E56  ee                   out dx, al                      
2E57  ee                   out dx, al                      
2E58  ee                   out dx, al                      
2E59  ee                   out dx, al                      
2E5A  ee                   out dx, al                      
2E5B  ee                   out dx, al                      
2E5C  eb 00                jmp 0x2e5e                      
2E5E  b0 aa                mov al, 0xaa                    
2E60  ee                   out dx, al                      
2E61  ee                   out dx, al                      
2E62  ee                   out dx, al                      
2E63  ee                   out dx, al                      
2E64  ee                   out dx, al                      
2E65  ee                   out dx, al                      
2E66  ee                   out dx, al                      
2E67  ee                   out dx, al                      
2E68  eb 00                jmp 0x2e6a                      
2E6A  b0 55                mov al, 0x55                    
2E6C  ee                   out dx, al                      
2E6D  ee                   out dx, al                      
2E6E  ee                   out dx, al                      
2E6F  ee                   out dx, al                      
2E70  ee                   out dx, al                      
2E71  ee                   out dx, al                      
2E72  ee                   out dx, al                      
2E73  ee                   out dx, al                      
2E74  eb 00                jmp 0x2e76                      
2E76  b0 00                mov al, 0                       
2E78  ee                   out dx, al                      
2E79  ee                   out dx, al                      
2E7A  ee                   out dx, al                      
2E7B  ee                   out dx, al                      
2E7C  ee                   out dx, al                      
2E7D  ee                   out dx, al                      
2E7E  ee                   out dx, al                      
2E7F  ee                   out dx, al                      
2E80  eb 00                jmp 0x2e82                      
2E82  b0 ff                mov al, 0xff                    
2E84  ee                   out dx, al                      
2E85  ee                   out dx, al                      
2E86  ee                   out dx, al                      
2E87  ee                   out dx, al                      
2E88  ee                   out dx, al                      
2E89  ee                   out dx, al                      
2E8A  ee                   out dx, al                      
2E8B  ee                   out dx, al                      
2E8C  eb 00                jmp 0x2e8e                      
2E8E  42                   inc dx                          
2E8F  ec                   in al, dx                       
2E90  24 f0                and al, 0xf0                    
2E92  3c b0                cmp al, 0xb0                    
2E94  b8 ff ff             mov ax, 0xffff                  
2E97  75 49                jne 0x2ee2                      
2E99  4a                   dec dx                          
2E9A  b0 87                mov al, 0x87                    
2E9C  ee                   out dx, al                      
2E9D  ee                   out dx, al                      
2E9E  ee                   out dx, al                      
2E9F  ee                   out dx, al                      
2EA0  ee                   out dx, al                      
2EA1  ee                   out dx, al                      
2EA2  ee                   out dx, al                      
2EA3  ee                   out dx, al                      
2EA4  42                   inc dx                          
2EA5  eb 00                jmp 0x2ea7                      
2EA7  ec                   in al, dx                       
2EA8  24 f0                and al, 0xf0                    
2EAA  3c 50                cmp al, 0x50                    
2EAC  b8 ff ff             mov ax, 0xffff                  
2EAF  75 31                jne 0x2ee2                      
2EB1  4a                   dec dx                          
2EB2  b0 78                mov al, 0x78                    
2EB4  ee                   out dx, al                      
2EB5  ee                   out dx, al                      
2EB6  ee                   out dx, al                      
2EB7  ee                   out dx, al                      
2EB8  ee                   out dx, al                      
2EB9  ee                   out dx, al                      
2EBA  ee                   out dx, al                      
2EBB  ee                   out dx, al                      
2EBC  42                   inc dx                          
2EBD  eb 00                jmp 0x2ebf                      
2EBF  ec                   in al, dx                       
2EC0  24 b0                and al, 0xb0                    
2EC2  3c b0                cmp al, 0xb0                    
2EC4  b8 ff ff             mov ax, 0xffff                  
2EC7  75 19                jne 0x2ee2                      
2EC9  4a                   dec dx                          
2ECA  b0 08                mov al, 8                       
2ECC  ee                   out dx, al                      
2ECD  ee                   out dx, al                      
2ECE  ee                   out dx, al                      
2ECF  ee                   out dx, al                      
2ED0  ee                   out dx, al                      
2ED1  ee                   out dx, al                      
2ED2  ee                   out dx, al                      
2ED3  ee                   out dx, al                      
2ED4  b0 ff                mov al, 0xff                    
2ED6  ee                   out dx, al                      
2ED7  ee                   out dx, al                      
2ED8  ee                   out dx, al                      
2ED9  ee                   out dx, al                      
2EDA  ee                   out dx, al                      
2EDB  ee                   out dx, al                      
2EDC  ee                   out dx, al                      
2EDD  ee                   out dx, al                      
2EDE  eb 00                jmp 0x2ee0                      
2EE0  33 c0                xor ax, ax                      
2EE2  5a                   pop dx                          
2EE3  c3                   ret                             
2EE4  52                   push dx                         
2EE5  9c                   pushf                           
2EE6  fa                   cli                             
2EE7  80 3e 00 0c 01       cmp byte ptr [0xc00], 1         
2EEC  74 65                je 0x2f53                       
2EEE  33 c0                xor ax, ax                      
2EF0  80 3e e9 0b 01       cmp byte ptr [0xbe9], 1         
2EF5  74 43                je 0x2f3a                       
2EF7  b8 ff ff             mov ax, 0xffff                  
2EFA  80 3e 01 0c 01       cmp byte ptr [0xc01], 1         
2EFF  74 4f                je 0x2f50                       
2F01  c6 06 01 0c 01       mov byte ptr [0xc01], 1         
2F06  c6 06 1c 0c 00       mov byte ptr [0xc1c], 0         
2F0B  e8 71 20             call 0x4f7f                     
2F0E  50                   push ax                         
2F0F  e8 5f 08             call 0x3771                     
2F12  c6 06 11 0c 01       mov byte ptr [0xc11], 1         
2F17  0b c0                or ax, ax                       
2F19  74 05                je 0x2f20                       
2F1B  c6 06 11 0c 00       mov byte ptr [0xc11], 0         
2F20  58                   pop ax                          
2F21  c6 06 01 0c 00       mov byte ptr [0xc01], 0         
2F26  b8 fe ff             mov ax, 0xfffe                  
2F29  80 3e 11 0c 01       cmp byte ptr [0xc11], 1         
2F2E  75 20                jne 0x2f50                      
2F30  c6 06 01 0c 01       mov byte ptr [0xc01], 1         
2F35  33 c0                xor ax, ax                      
2F37  e9 fe 00             jmp 0x3038                      
2F3A  80 3e d7 0b 0c       cmp byte ptr [0xbd7], 0xc          ; 000C='SIMGR'
2F3F  75 0f                jne 0x2f50                      
2F41  52                   push dx                         
2F42  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
2F46  81 c2 02 04          add dx, 0x402                   
2F4A  b0 34                mov al, 0x34                    
2F4C  ee                   out dx, al                      
2F4D  5a                   pop dx                          
2F4E  33 c0                xor ax, ax                      
2F50  e9 e5 00             jmp 0x3038                      
2F53  c6 06 1c 0c 00       mov byte ptr [0xc1c], 0         
2F58  b8 ff ff             mov ax, 0xffff                  
2F5B  80 3e 01 0c 01       cmp byte ptr [0xc01], 1         
2F60  75 03                jne 0x2f65                      
2F62  e9 d3 00             jmp 0x3038                      
2F65  c6 06 01 0c 01       mov byte ptr [0xc01], 1         
2F6A  e8 12 20             call 0x4f7f                     
2F6D  50                   push ax                         
2F6E  e8 00 08             call 0x3771                     
2F71  c6 06 11 0c 01       mov byte ptr [0xc11], 1         
2F76  0b c0                or ax, ax                       
2F78  74 05                je 0x2f7f                       
2F7A  c6 06 11 0c 00       mov byte ptr [0xc11], 0         
2F7F  58                   pop ax                          
2F80  80 3e 11 0c 01       cmp byte ptr [0xc11], 1         
2F85  75 02                jne 0x2f89                      
2F87  eb 5a                jmp 0x2fe3                      
2F89  80 3e 00 0c 01       cmp byte ptr [0xc00], 1         
2F8E  75 2c                jne 0x2fbc                      
2F90  c6 06 00 0c 00       mov byte ptr [0xc00], 0         
2F95  e8 aa fc             call 0x2c42                     
2F98  80 3e 00 0c 01       cmp byte ptr [0xc00], 1         
2F9D  c6 06 00 0c 01       mov byte ptr [0xc00], 1         
2FA2  74 18                je 0x2fbc                       
2FA4  ba 0d 00             mov dx, 0xd                        ; 000D='IMGR'
2FA7  a0 f5 0b             mov al, byte ptr [0xbf5]        
2FAA  0c 20                or al, 0x20                     
2FAC  e8 7b fb             call 0x2b2a                     
2FAF  c6 06 00 0c 00       mov byte ptr [0xc00], 0         
2FB4  e8 8b fc             call 0x2c42                     
2FB7  c6 06 00 0c 01       mov byte ptr [0xc00], 1         
2FBC  e8 94 fb             call 0x2b53                     
2FBF  50                   push ax                         
2FC0  e8 ae 07             call 0x3771                     
2FC3  c6 06 11 0c 01       mov byte ptr [0xc11], 1         
2FC8  0b c0                or ax, ax                       
2FCA  58                   pop ax                          
2FCB  74 05                je 0x2fd2                       
2FCD  c6 06 11 0c 00       mov byte ptr [0xc11], 0         
2FD2  80 3e 11 0c 01       cmp byte ptr [0xc11], 1         
2FD7  74 0a                je 0x2fe3                       
2FD9  c6 06 01 0c 00       mov byte ptr [0xc01], 0         
2FDE  b8 fe ff             mov ax, 0xfffe                  
2FE1  eb 55                jmp 0x3038                      
2FE3  33 c0                xor ax, ax                      
2FE5  80 3e 00 0c 01       cmp byte ptr [0xc00], 1         
2FEA  75 4c                jne 0x3038                      
2FEC  ba 0d 00             mov dx, 0xd                        ; 000D='IMGR'
2FEF  e8 59 f4             call 0x244b                     
2FF2  8a 26 f5 0b          mov ah, byte ptr [0xbf5]        
2FF6  80 e4 20             and ah, 0x20                    
2FF9  0a c4                or al, ah                       
2FFB  0c 43                or al, 0x43                     
2FFD  24 eb                and al, 0xeb                    
2FFF  e8 70 f4             call 0x2472                     
3002  ba 0c 00             mov dx, 0xc                        ; 000C='SIMGR'
3005  a0 f3 0b             mov al, byte ptr [0xbf3]        
3008  e8 67 f4             call 0x2472                     
300B  80 3e f4 0b ff       cmp byte ptr [0xbf4], 0xff      
3010  74 12                je 0x3024                       
3012  ba 12 00             mov dx, 0x12                    
3015  a0 f4 0b             mov al, byte ptr [0xbf4]        
3018  80 3e 17 0c 01       cmp byte ptr [0xc17], 1         
301D  75 02                jne 0x3021                      
301F  0c 80                or al, 0x80                     
3021  e8 4e f4             call 0x2472                     
3024  ba 0a 00             mov dx, 0xa                        ; 000A='SCSIMGR'
3027  a0 f6 0b             mov al, byte ptr [0xbf6]        
302A  e8 45 f4             call 0x2472                     
302D  ba 08 00             mov dx, 8                       
3030  a0 07 0c             mov al, byte ptr [0xc07]        
3033  e8 3c f4             call 0x2472                     
3036  33 c0                xor ax, ax                      
3038  5a                   pop dx                          
3039  f6 c6 02             test dh, 2                      
303C  74 01                je 0x303f                       
303E  fb                   sti                             
303F  5a                   pop dx                          
3040  c3                   ret                             
3041  52                   push dx                         
3042  9c                   pushf                           
3043  fa                   cli                             
3044  c6 06 1c 0c 00       mov byte ptr [0xc1c], 0         
3049  80 3e 00 0c 00       cmp byte ptr [0xc00], 0         
304E  74 02                je 0x3052                       
3050  eb 43                jmp 0x3095                      
3052  80 3e e9 0b 01       cmp byte ptr [0xbe9], 1         
3057  74 02                je 0x305b                       
3059  eb 24                jmp 0x307f                      
305B  80 3e d7 0b 0c       cmp byte ptr [0xbd7], 0xc          ; 000C='SIMGR'
3060  74 02                je 0x3064                       
3062  eb 11                jmp 0x3075                      
3064  52                   push dx                         
3065  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
3069  81 c2 02 04          add dx, 0x402                   
306D  b0 64                mov al, 0x64                    
306F  ee                   out dx, al                      
3070  5a                   pop dx                          
3071  33 c0                xor ax, ax                      
3073  eb 57                jmp 0x30cc                      
3075  b8 01 00             mov ax, 1                       
3078  e8 89 f4             call 0x2504                     
307B  33 c0                xor ax, ax                      
307D  eb 4d                jmp 0x30cc                      
307F  b8 ff ff             mov ax, 0xffff                  
3082  80 3e 01 0c 00       cmp byte ptr [0xc01], 0         
3087  74 43                je 0x30cc                       
3089  c6 06 01 0c 00       mov byte ptr [0xc01], 0         
308E  e8 53 23             call 0x53e4                     
3091  33 c0                xor ax, ax                      
3093  eb 37                jmp 0x30cc                      
3095  b8 ff ff             mov ax, 0xffff                  
3098  80 3e 01 0c 00       cmp byte ptr [0xc01], 0         
309D  74 2d                je 0x30cc                       
309F  c6 06 01 0c 00       mov byte ptr [0xc01], 0         
30A4  ba 0d 00             mov dx, 0xd                        ; 000D='IMGR'
30A7  e8 a1 f3             call 0x244b                     
30AA  24 af                and al, 0xaf                    
30AC  e8 c3 f3             call 0x2472                     
30AF  e8 32 23             call 0x53e4                     
30B2  80 3e 0d 0c 01       cmp byte ptr [0xc0d], 1         
30B7  74 11                je 0x30ca                       
30B9  80 3e e9 0b 00       cmp byte ptr [0xbe9], 0         
30BE  74 0a                je 0x30ca                       
30C0  c6 06 13 0c 01       mov byte ptr [0xc13], 1         
30C5  b0 48                mov al, 0x48                    
30C7  e8 0f f6             call 0x26d9                     
30CA  33 c0                xor ax, ax                      
30CC  5a                   pop dx                          
30CD  f6 c6 02             test dh, 2                      
30D0  74 01                je 0x30d3                       
30D2  fb                   sti                             
30D3  5a                   pop dx                          
30D4  c3                   ret                             
30D5  52                   push dx                         
30D6  a2 8a 0c             mov byte ptr [0xc8a], al        
30D9  80 3e 01 0c 01       cmp byte ptr [0xc01], 1         
30DE  74 03                je 0x30e3                       
30E0  e8 01 fe             call 0x2ee4                     
30E3  e8 29 63             call 0x940f                     
30E6  e8 58 ff             call 0x3041                     
30E9  5a                   pop dx                          
30EA  c3                   ret                             
30EB  52                   push dx                         
30EC  50                   push ax                         
30ED  80 3e 01 0c 01       cmp byte ptr [0xc01], 1         
30F2  74 03                je 0x30f7                       
30F4  e8 ed fd             call 0x2ee4                     
30F7  ba 0d 00             mov dx, 0xd                        ; 000D='IMGR'
30FA  e8 4e f3             call 0x244b                     
30FD  24 df                and al, 0xdf                    
30FF  0c 03                or al, 3                        
3101  e8 6e f3             call 0x2472                     
3104  ba 12 00             mov dx, 0x12                    
3107  e8 41 f3             call 0x244b                     
310A  24 e3                and al, 0xe3                    
310C  ba 12 00             mov dx, 0x12                    
310F  c6 06 f4 0b ff       mov byte ptr [0xbf4], 0xff      
3114  c6 06 17 0c 00       mov byte ptr [0xc17], 0         
3119  e8 56 f3             call 0x2472                     
311C  c6 06 e9 0b 00       mov byte ptr [0xbe9], 0         
3121  ba 0a 00             mov dx, 0xa                        ; 000A='SCSIMGR'
3124  e8 24 f3             call 0x244b                     
3127  24 1f                and al, 0x1f                    
3129  e8 46 f3             call 0x2472                     
312C  c6 06 05 0c 00       mov byte ptr [0xc05], 0         
3131  ba 0c 00             mov dx, 0xc                        ; 000C='SIMGR'
3134  02 16 e8 0b          add dl, byte ptr [0xbe8]        
3138  e8 10 f3             call 0x244b                     
313B  50                   push ax                         
313C  32 c0                xor al, al                      
313E  c6 06 07 0c 00       mov byte ptr [0xc07], 0         
3143  ba 08 00             mov dx, 8                       
3146  e8 29 f3             call 0x2472                     
3149  58                   pop ax                          
314A  c6 06 00 0c 00       mov byte ptr [0xc00], 0         
314F  a8 80                test al, 0x80                   
3151  75 05                jne 0x3158                      
3153  c6 06 00 0c 01       mov byte ptr [0xc00], 1         
3158  e8 89 22             call 0x53e4                     
315B  80 3e d7 0b 0c       cmp byte ptr [0xbd7], 0xc          ; 000C='SIMGR'
3160  75 16                jne 0x3178                      
3162  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
3166  83 c2 02             add dx, 2                       
3169  b0 0c                mov al, 0xc                        ; 000C='SIMGR'
316B  ee                   out dx, al                      
316C  ee                   out dx, al                      
316D  ee                   out dx, al                      
316E  b0 0e                mov al, 0xe                     
3170  ee                   out dx, al                      
3171  ee                   out dx, al                      
3172  ee                   out dx, al                      
3173  b0 04                mov al, 4                       
3175  ee                   out dx, al                      
3176  ee                   out dx, al                      
3177  ee                   out dx, al                      
3178  a0 0e 0c             mov al, byte ptr [0xc0e]        
317B  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
317F  83 c2 02             add dx, 2                       
3182  ee                   out dx, al                      
3183  c6 06 01 0c 01       mov byte ptr [0xc01], 1         
3188  c6 06 c0 0b 00       mov byte ptr [0xbc0], 0         
318D  c6 06 c8 0b 00       mov byte ptr [0xbc8], 0         
3192  c6 06 0c 0c 00       mov byte ptr [0xc0c], 0         
3197  c6 06 fc 0b 05       mov byte ptr [0xbfc], 5         
319C  c6 06 d7 0b 00       mov byte ptr [0xbd7], 0         
31A1  c6 06 d8 0b 00       mov byte ptr [0xbd8], 0         
31A6  c6 06 dd 0b 00       mov byte ptr [0xbdd], 0         
31AB  c6 06 de 0b 00       mov byte ptr [0xbde], 0         
31B0  c7 06 1a 0c ff ff    mov word ptr [0xc1a], 0xffff    
31B6  80 26 08 0c df       and byte ptr [0xc08], 0xdf      
31BB  80 26 07 0c df       and byte ptr [0xc07], 0xdf      
31C0  c6 06 3c 0d 00       mov byte ptr [0xd3c], 0         
31C5  c6 06 14 0c 01       mov byte ptr [0xc14], 1         
31CA  58                   pop ax                          
31CB  5a                   pop dx                          
31CC  c3                   ret                             
31CD  52                   push dx                         
31CE  ba 0c 00             mov dx, 0xc                        ; 000C='SIMGR'
31D1  e8 77 f2             call 0x244b                     
31D4  0c 40                or al, 0x40                     
31D6  e8 99 f2             call 0x2472                     
31D9  33 c0                xor ax, ax                      
31DB  5a                   pop dx                          
31DC  c3                   ret                             
31DD  52                   push dx                         
31DE  ba 0e 00             mov dx, 0xe                     
31E1  b0 02                mov al, 2                       
31E3  e8 8c f2             call 0x2472                     
31E6  ba 0f 00             mov dx, 0xf                     
31E9  e8 5f f2             call 0x244b                     
31EC  32 e4                xor ah, ah                      
31EE  5a                   pop dx                          
31EF  c3                   ret                             
31F0  56                   push si                         
31F1  52                   push dx                         
31F2  f7 06 e9 09 80 00    test word ptr [0x9e9], 0x80     
31F8  75 07                jne 0x3201                      
31FA  80 3e eb 09 01       cmp byte ptr [0x9eb], 1         
31FF  75 3d                jne 0x323e                      
3201  83 fa ff             cmp dx, -1                      
3204  75 05                jne 0x320b                      
3206  83 f8 ff             cmp ax, -1                      
3209  74 33                je 0x323e                       
320B  52                   push dx                         
320C  50                   push ax                         
320D  ba 0a 00             mov dx, 0xa                        ; 000A='SCSIMGR'
3210  b0 10                mov al, 0x10                    
3212  e8 5d f2             call 0x2472                     
3215  58                   pop ax                          
3216  5a                   pop dx                          
3217  51                   push cx                         
3218  b9 00 02             mov cx, 0x200                   
321B  f7 f1                div cx                          
321D  59                   pop cx                          
321E  50                   push ax                         
321F  b2 0e                mov dl, 0xe                     
3221  b0 04                mov al, 4                       
3223  e8 4c f2             call 0x2472                     
3226  b2 0f                mov dl, 0xf                     
3228  58                   pop ax                          
3229  50                   push ax                         
322A  e8 45 f2             call 0x2472                     
322D  b2 0e                mov dl, 0xe                     
322F  b0 05                mov al, 5                       
3231  e8 3e f2             call 0x2472                     
3234  b2 0f                mov dl, 0xf                     
3236  58                   pop ax                          
3237  8a c4                mov al, ah                      
3239  24 03                and al, 3                       
323B  e8 34 f2             call 0x2472                     
323E  f7 c3 01 00          test bx, 1                      
3242  74 2b                je 0x326f                       
3244  53                   push bx                         
3245  ba 0c 00             mov dx, 0xc                        ; 000C='SIMGR'
3248  e8 00 f2             call 0x244b                     
324B  24 e7                and al, 0xe7                    
324D  0c 40                or al, 0x40                     
324F  f7 06 e9 09 40 00    test word ptr [0x9e9], 0x40     
3255  75 0a                jne 0x3261                      
3257  f7 06 e9 09 80 00    test word ptr [0x9e9], 0x80     
325D  74 02                je 0x3261                       
325F  0c 20                or al, 0x20                     
3261  f7 06 e9 09 40 00    test word ptr [0x9e9], 0x40     
3267  74 02                je 0x326b                       
3269  0c 18                or al, 0x18                     
326B  e8 04 f2             call 0x2472                     
326E  5b                   pop bx                          
326F  57                   push di                         
3270  8b fe                mov di, si                      
3272  e8 40 f2             call 0x24b5                     
3275  f7 c3 01 00          test bx, 1                      
3279  b8 00 00             mov ax, 0                       
327C  74 1c                je 0x329a                       
327E  b0 00                mov al, 0                       
3280  26 32 05             xor al, byte ptr es:[di]        
3283  47                   inc di                          
3284  49                   dec cx                          
3285  75 f9                jne 0x3280                      
3287  8a c8                mov cl, al                      
3289  b2 0e                mov dl, 0xe                     
328B  b0 02                mov al, 2                       
328D  e8 e2 f1             call 0x2472                     
3290  b2 0f                mov dl, 0xf                     
3292  e8 b6 f1             call 0x244b                     
3295  32 c1                xor al, cl                      
3297  25 ff 00             and ax, 0xff                    
329A  5f                   pop di                          
329B  5a                   pop dx                          
329C  5e                   pop si                          
329D  c3                   ret                             
329E  57                   push di                         
329F  52                   push dx                         
32A0  f7 06 e9 09 80 00    test word ptr [0x9e9], 0x80     
32A6  75 07                jne 0x32af                      
32A8  80 3e eb 09 01       cmp byte ptr [0x9eb], 1         
32AD  75 48                jne 0x32f7                      
32AF  83 fa ff             cmp dx, -1                      
32B2  75 05                jne 0x32b9                      
32B4  83 f8 ff             cmp ax, -1                      
32B7  74 3e                je 0x32f7                       
32B9  52                   push dx                         
32BA  50                   push ax                         
32BB  ba 0a 00             mov dx, 0xa                        ; 000A='SCSIMGR'
32BE  b0 10                mov al, 0x10                    
32C0  e8 af f1             call 0x2472                     
32C3  58                   pop ax                          
32C4  5a                   pop dx                          
32C5  51                   push cx                         
32C6  b9 00 02             mov cx, 0x200                   
32C9  f7 f1                div cx                          
32CB  59                   pop cx                          
32CC  50                   push ax                         
32CD  b2 0e                mov dl, 0xe                     
32CF  b0 04                mov al, 4                       
32D1  e8 9e f1             call 0x2472                     
32D4  b2 0f                mov dl, 0xf                     
32D6  58                   pop ax                          
32D7  50                   push ax                         
32D8  e8 97 f1             call 0x2472                     
32DB  b2 0e                mov dl, 0xe                     
32DD  b0 05                mov al, 5                       
32DF  e8 90 f1             call 0x2472                     
32E2  b2 0f                mov dl, 0xf                     
32E4  58                   pop ax                          
32E5  8a c4                mov al, ah                      
32E7  24 03                and al, 3                       
32E9  e8 86 f1             call 0x2472                     
32EC  ba 0c 00             mov dx, 0xc                        ; 000C='SIMGR'
32EF  e8 59 f1             call 0x244b                     
32F2  24 ef                and al, 0xef                    
32F4  e8 7b f1             call 0x2472                     
32F7  f7 c3 01 00          test bx, 1                      
32FB  74 2b                je 0x3328                       
32FD  53                   push bx                         
32FE  ba 0c 00             mov dx, 0xc                        ; 000C='SIMGR'
3301  e8 47 f1             call 0x244b                     
3304  24 e7                and al, 0xe7                    
3306  0c 40                or al, 0x40                     
3308  f7 06 e9 09 40 00    test word ptr [0x9e9], 0x40     
330E  75 0a                jne 0x331a                      
3310  f7 06 e9 09 80 00    test word ptr [0x9e9], 0x80     
3316  74 02                je 0x331a                       
3318  0c 20                or al, 0x20                     
331A  f7 06 e9 09 40 00    test word ptr [0x9e9], 0x40     
3320  74 02                je 0x3324                       
3322  0c 18                or al, 0x18                     
3324  e8 4b f1             call 0x2472                     
3327  5b                   pop bx                          
3328  56                   push si                         
3329  8b f7                mov si, di                      
332B  e8 6d f1             call 0x249b                     
332E  f7 c3 01 00          test bx, 1                      
3332  b8 00 00             mov ax, 0                       
3335  74 1c                je 0x3353                       
3337  b0 00                mov al, 0                       
3339  26 32 04             xor al, byte ptr es:[si]        
333C  46                   inc si                          
333D  49                   dec cx                          
333E  75 f9                jne 0x3339                      
3340  8a c8                mov cl, al                      
3342  b2 0e                mov dl, 0xe                     
3344  b0 02                mov al, 2                       
3346  e8 29 f1             call 0x2472                     
3349  b2 0f                mov dl, 0xf                     
334B  e8 fd f0             call 0x244b                     
334E  32 c1                xor al, cl                      
3350  25 ff 00             and ax, 0xff                    
3353  5e                   pop si                          
3354  5a                   pop dx                          
3355  5f                   pop di                          
3356  c3                   ret                             
3357  50                   push ax                         
3358  52                   push dx                         
3359  f7 06 e9 09 40 00    test word ptr [0x9e9], 0x40     
335F  74 19                je 0x337a                       
3361  51                   push cx                         
3362  52                   push dx                         
3363  50                   push ax                         
3364  ba 0a 00             mov dx, 0xa                        ; 000A='SCSIMGR'
3367  b0 10                mov al, 0x10                    
3369  e8 06 f1             call 0x2472                     
336C  ba 0c 00             mov dx, 0xc                        ; 000C='SIMGR'
336F  e8 d9 f0             call 0x244b                     
3372  24 ef                and al, 0xef                    
3374  e8 fb f0             call 0x2472                     
3377  58                   pop ax                          
3378  5a                   pop dx                          
3379  59                   pop cx                          
337A  83 fa ff             cmp dx, -1                      
337D  74 29                je 0x33a8                       
337F  51                   push cx                         
3380  b9 00 02             mov cx, 0x200                   
3383  f7 f1                div cx                          
3385  59                   pop cx                          
3386  50                   push ax                         
3387  ba 0e 00             mov dx, 0xe                     
338A  b0 06                mov al, 6                       
338C  e8 e3 f0             call 0x2472                     
338F  b2 0f                mov dl, 0xf                     
3391  58                   pop ax                          
3392  50                   push ax                         
3393  e8 dc f0             call 0x2472                     
3396  ba 0e 00             mov dx, 0xe                     
3399  b0 07                mov al, 7                       
339B  e8 d4 f0             call 0x2472                     
339E  b2 0f                mov dl, 0xf                     
33A0  58                   pop ax                          
33A1  8a c4                mov al, ah                      
33A3  24 03                and al, 3                       
33A5  e8 ca f0             call 0x2472                     
33A8  8b c1                mov ax, cx                      
33AA  86 e0                xchg al, ah                     
33AC  32 e4                xor ah, ah                      
33AE  d0 e8                shr al, 1                       
33B0  48                   dec ax                          
33B1  ba 14 00             mov dx, 0x14                    
33B4  e8 bb f0             call 0x2472                     
33B7  ba 12 00             mov dx, 0x12                    
33BA  e8 8e f0             call 0x244b                     
33BD  83 fe 01             cmp si, 1                       
33C0  75 04                jne 0x33c6                      
33C2  24 fe                and al, 0xfe                    
33C4  eb 02                jmp 0x33c8                      
33C6  0c 01                or al, 1                        
33C8  0c 02                or al, 2                        
33CA  b2 12                mov dl, 0x12                    
33CC  a2 f4 0b             mov byte ptr [0xbf4], al        
33CF  f7 06 e9 09 00 02    test word ptr [0x9e9], 0x200    
33D5  74 07                je 0x33de                       
33D7  0c 80                or al, 0x80                     
33D9  c6 06 17 0c 01       mov byte ptr [0xc17], 1         
33DE  e8 91 f0             call 0x2472                     
33E1  5a                   pop dx                          
33E2  58                   pop ax                          
33E3  c3                   ret                             
33E4  50                   push ax                         
33E5  52                   push dx                         
33E6  ba 12 00             mov dx, 0x12                    
33E9  e8 5f f0             call 0x244b                     
33EC  ba 12 00             mov dx, 0x12                    
33EF  24 fc                and al, 0xfc                    
33F1  a2 f4 0b             mov byte ptr [0xbf4], al        
33F4  c6 06 17 0c 00       mov byte ptr [0xc17], 0         
33F9  e8 76 f0             call 0x2472                     
33FC  f7 06 e9 09 40 00    test word ptr [0x9e9], 0x40     
3402  74 13                je 0x3417                       
3404  ba 0a 00             mov dx, 0xa                        ; 000A='SCSIMGR'
3407  b0 18                mov al, 0x18                    
3409  e8 66 f0             call 0x2472                     
340C  ba 0c 00             mov dx, 0xc                        ; 000C='SIMGR'
340F  e8 39 f0             call 0x244b                     
3412  0c 10                or al, 0x10                     
3414  e8 5b f0             call 0x2472                     
3417  5a                   pop dx                          
3418  58                   pop ax                          
3419  c3                   ret                             
341A  52                   push dx                         
341B  83 ca 18             or dx, 0x18                     
341E  83 fa 1e             cmp dx, 0x1e                    
3421  72 03                jb 0x3426                       
3423  83 e2 f7             and dx, 0xfff7                  
3426  e8 22 f0             call 0x244b                     
3429  5a                   pop dx                          
342A  c3                   ret                             
342B  52                   push dx                         
342C  50                   push ax                         
342D  83 ca 18             or dx, 0x18                     
3430  83 fa 1e             cmp dx, 0x1e                    
3433  72 03                jb 0x3438                       
3435  83 e2 f7             and dx, 0xfff7                  
3438  e8 37 f0             call 0x2472                     
343B  58                   pop ax                          
343C  5a                   pop dx                          
343D  c3                   ret                             
343E  52                   push dx                         
343F  83 fa 08             cmp dx, 8                       
3442  75 05                jne 0x3449                      
3444  ba 16 00             mov dx, 0x16                    
3447  eb 0d                jmp 0x3456                      
3449  83 fa 09             cmp dx, 9                       
344C  75 05                jne 0x3453                      
344E  ba 17 00             mov dx, 0x17                    
3451  eb 03                jmp 0x3456                      
3453  83 ca 18             or dx, 0x18                     
3456  80 3e eb 09 01       cmp byte ptr [0x9eb], 1         
345B  75 08                jne 0x3465                      
345D  83 fa 1e             cmp dx, 0x1e                    
3460  72 03                jb 0x3465                       
3462  83 e2 f7             and dx, 0xfff7                  
3465  e8 e3 ef             call 0x244b                     
3468  5a                   pop dx                          
3469  c3                   ret                             
346A  52                   push dx                         
346B  50                   push ax                         
346C  83 fa 08             cmp dx, 8                       
346F  75 05                jne 0x3476                      
3471  ba 16 00             mov dx, 0x16                    
3474  eb 0d                jmp 0x3483                      
3476  83 fa 09             cmp dx, 9                       
3479  75 05                jne 0x3480                      
347B  ba 17 00             mov dx, 0x17                    
347E  eb 03                jmp 0x3483                      
3480  83 ca 18             or dx, 0x18                     
3483  80 3e eb 09 01       cmp byte ptr [0x9eb], 1         
3488  75 08                jne 0x3492                      
348A  83 fa 1e             cmp dx, 0x1e                    
348D  72 03                jb 0x3492                       
348F  83 e2 f7             and dx, 0xfff7                  
3492  e8 dd ef             call 0x2472                     
3495  58                   pop ax                          
3496  5a                   pop dx                          
3497  c3                   ret                             
3498  33 c0                xor ax, ax                      
349A  a1 fa 0b             mov ax, word ptr [0xbfa]        
349D  c3                   ret                             
349E  33 c0                xor ax, ax                      
34A0  a0 fc 0b             mov al, byte ptr [0xbfc]        
34A3  f6 06 07 0c 20       test byte ptr [0xc07], 0x20     
34A8  74 02                je 0x34ac                       
34AA  0c 40                or al, 0x40                     
34AC  c3                   ret                             
34AD  3c ff                cmp al, 0xff                    
34AF  74 0f                je 0x34c0                       
34B1  a2 0b 0c             mov byte ptr [0xc0b], al        
34B4  f6 06 0b 0c 0f       test byte ptr [0xc0b], 0xf      
34B9  74 05                je 0x34c0                       
34BB  24 0f                and al, 0xf                     
34BD  a2 fc 0b             mov byte ptr [0xbfc], al        
34C0  c3                   ret                             
34C1  a2 c0 0b             mov byte ptr [0xbc0], al        
34C4  a2 0c 0c             mov byte ptr [0xc0c], al        
34C7  c3                   ret                             
34C8  c7 06 e9 09 80 00    mov word ptr [0x9e9], 0x80      
34CE  c6 06 eb 09 01       mov byte ptr [0x9eb], 1         
34D3  83 f8 00             cmp ax, 0                       
34D6  74 55                je 0x352d                       
34D8  83 f8 ff             cmp ax, -1                      
34DB  74 50                je 0x352d                       
34DD  c7 06 e9 09 c0 00    mov word ptr [0x9e9], 0xc0      
34E3  c6 06 eb 09 00       mov byte ptr [0x9eb], 0         
34E8  50                   push ax                         
34E9  83 e0 05             and ax, 5                       
34EC  83 f8 05             cmp ax, 5                       
34EF  58                   pop ax                          
34F0  74 3b                je 0x352d                       
34F2  c7 06 e9 09 40 00    mov word ptr [0x9e9], 0x40      
34F8  c6 06 eb 09 00       mov byte ptr [0x9eb], 0         
34FD  a9 01 00             test ax, 1                      
3500  75 2b                jne 0x352d                      
3502  c7 06 e9 09 80 00    mov word ptr [0x9e9], 0x80      
3508  c6 06 eb 09 01       mov byte ptr [0x9eb], 1         
350D  a9 04 00             test ax, 4                      
3510  75 1b                jne 0x352d                      
3512  c7 06 e9 09 00 00    mov word ptr [0x9e9], 0         
3518  c6 06 eb 09 00       mov byte ptr [0x9eb], 0         
351D  a9 02 00             test ax, 2                      
3520  75 0b                jne 0x352d                      
3522  c6 06 eb 09 01       mov byte ptr [0x9eb], 1         
3527  c7 06 e9 09 80 00    mov word ptr [0x9e9], 0x80      
352D  a9 08 00             test ax, 8                      
3530  74 06                je 0x3538                       
3532  81 0e e9 09 00 02    or word ptr [0x9e9], 0x200      
3538  c3                   ret                             
3539  c6 06 0d 0c 01       mov byte ptr [0xc0d], 1         
353E  c3                   ret                             
353F  c6 06 0d 0c 00       mov byte ptr [0xc0d], 0         
3544  c3                   ret                             
3545  a0 c0 0b             mov al, byte ptr [0xbc0]        
3548  c3                   ret                             
3549  0c 08                or al, 8                        
354B  e8 8b f1             call 0x26d9                     
354E  a0 bd 0b             mov al, byte ptr [0xbbd]        
3551  c3                   ret                             
3552  50                   push ax                         
3553  0c 50                or al, 0x50                     
3555  e8 81 f1             call 0x26d9                     
3558  58                   pop ax                          
3559  c3                   ret                             
355A  50                   push ax                         
355B  b0 48                mov al, 0x48                    
355D  e8 79 f1             call 0x26d9                     
3560  58                   pop ax                          
3561  c3                   ret                             
3562  50                   push ax                         
3563  b0 40                mov al, 0x40                    
3565  e8 71 f1             call 0x26d9                     
3568  58                   pop ax                          
3569  c3                   ret                             
356A  52                   push dx                         
356B  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
356F  42                   inc dx                          
3570  ec                   in al, dx                       
3571  8a d0                mov dl, al                      
3573  e8 28 ff             call 0x349e                     
3576  24 40                and al, 0x40                    
3578  32 d0                xor dl, al                      
357A  80 3e 00 0c 01       cmp byte ptr [0xc00], 1         
357F  75 0d                jne 0x358e                      
3581  80 3e 01 0c 01       cmp byte ptr [0xc01], 1         
3586  b8 ff ff             mov ax, 0xffff                  
3589  75 11                jne 0x359c                      
358B  e9 85 00             jmp 0x3613                      
358E  80 3e d7 0b 0a       cmp byte ptr [0xbd7], 0xa          ; 000A='SCSIMGR'
3593  74 07                je 0x359c                       
3595  80 3e d7 0b 0b       cmp byte ptr [0xbd7], 0xb          ; 000B='CSIMGR'
359A  75 02                jne 0x359e                      
359C  eb 58                jmp 0x35f6                      
359E  80 3e d7 0b 0c       cmp byte ptr [0xbd7], 0xc          ; 000C='SIMGR'
35A3  75 51                jne 0x35f6                      
35A5  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
35A9  81 c2 02 04          add dx, 0x402                   
35AD  ec                   in al, dx                       
35AE  a8 40                test al, 0x40                   
35B0  b8 ff ff             mov ax, 0xffff                  
35B3  74 5e                je 0x3613                       
35B5  80 3e 1c 0c 01       cmp byte ptr [0xc1c], 1         
35BA  74 12                je 0x35ce                       
35BC  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
35C0  42                   inc dx                          
35C1  ec                   in al, dx                       
35C2  c6 06 1c 0c 01       mov byte ptr [0xc1c], 1         
35C7  a8 08                test al, 8                      
35C9  b8 ff ff             mov ax, 0xffff                  
35CC  75 0a                jne 0x35d8                      
35CE  c6 06 1c 0c 00       mov byte ptr [0xc1c], 0         
35D3  b8 00 00             mov ax, 0                       
35D6  eb 3b                jmp 0x3613                      
35D8  50                   push ax                         
35D9  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
35DD  81 c2 02 04          add dx, 0x402                   
35E1  b0 34                mov al, 0x34                    
35E3  ee                   out dx, al                      
35E4  ee                   out dx, al                      
35E5  eb 00                jmp 0x35e7                      
35E7  b0 74                mov al, 0x74                    
35E9  ee                   out dx, al                      
35EA  ee                   out dx, al                      
35EB  eb 00                jmp 0x35ed                      
35ED  b0 64                mov al, 0x64                    
35EF  ee                   out dx, al                      
35F0  ee                   out dx, al                      
35F1  eb 00                jmp 0x35f3                      
35F3  58                   pop ax                          
35F4  eb 1d                jmp 0x3613                      
35F6  b8 ff ff             mov ax, 0xffff                  
35F9  f6 c2 40             test dl, 0x40                   
35FC  74 15                je 0x3613                       
35FE  b8 00 00             mov ax, 0                       
3601  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
3605  83 c2 02             add dx, 2                       
3608  ec                   in al, dx                       
3609  a8 10                test al, 0x10                   
360B  b8 ff ff             mov ax, 0xffff                  
360E  74 03                je 0x3613                       
3610  b8 00 00             mov ax, 0                       
3613  5a                   pop dx                          
3614  c3                   ret                             
3615  a1 e6 0b             mov ax, word ptr [0xbe6]        
3618  c3                   ret                             
3619  53                   push bx                         
361A  52                   push dx                         
361B  ba 12 00             mov dx, 0x12                    
361E  e8 2a ee             call 0x244b                     
3621  8a d8                mov bl, al                      
3623  a8 10                test al, 0x10                   
3625  74 07                je 0x362e                       
3627  a8 20                test al, 0x20                   
3629  b8 01 00             mov ax, 1                       
362C  75 5b                jne 0x3689                      
362E  f6 c3 08             test bl, 8                      
3631  b8 02 00             mov ax, 2                       
3634  75 53                jne 0x3689                      
3636  80 3e 16 0c 00       cmp byte ptr [0xc16], 0         
363B  74 23                je 0x3660                       
363D  ba 09 00             mov dx, 9                       
3640  e8 08 ee             call 0x244b                     
3643  8a d8                mov bl, al                      
3645  80 e3 f0             and bl, 0xf0                    
3648  80 fb f0             cmp bl, 0xf0                    
364B  b8 18 00             mov ax, 0x18                    
364E  74 39                je 0x3689                       
3650  80 fb 50             cmp bl, 0x50                    
3653  b8 08 00             mov ax, 8                       
3656  74 31                je 0x3689                       
3658  80 fb a0             cmp bl, 0xa0                    
365B  b8 10 00             mov ax, 0x10                    
365E  74 29                je 0x3689                       
3660  ba 0d 00             mov dx, 0xd                        ; 000D='IMGR'
3663  e8 e5 ed             call 0x244b                     
3666  24 fc                and al, 0xfc                    
3668  8a d8                mov bl, al                      
366A  ba 08 00             mov dx, 8                       
366D  e8 db ed             call 0x244b                     
3670  a8 01                test al, 1                      
3672  74 03                je 0x3677                       
3674  80 cb 01             or bl, 1                        
3677  a8 02                test al, 2                      
3679  74 03                je 0x367e                       
367B  80 cb 02             or bl, 2                        
367E  8a c3                mov al, bl                      
3680  ba 0d 00             mov dx, 0xd                        ; 000D='IMGR'
3683  e8 ec ed             call 0x2472                     
3686  b8 00 40             mov ax, 0x4000                  
3689  5a                   pop dx                          
368A  5b                   pop bx                          
368B  c3                   ret                             
368C  52                   push dx                         
368D  50                   push ax                         
368E  51                   push cx                         
368F  80 3e 01 0c 01       cmp byte ptr [0xc01], 1         
3694  74 03                je 0x3699                       
3696  e8 4b f8             call 0x2ee4                     
3699  ba 12 00             mov dx, 0x12                    
369C  e8 ac ed             call 0x244b                     
369F  a8 02                test al, 2                      
36A1  74 1a                je 0x36bd                       
36A3  ba 13 00             mov dx, 0x13                    
36A6  e8 a2 ed             call 0x244b                     
36A9  0c 02                or al, 2                        
36AB  c6 06 17 0c 00       mov byte ptr [0xc17], 0         
36B0  e8 bf ed             call 0x2472                     
36B3  b9 00 08             mov cx, 0x800                   
36B6  e8 60 ff             call 0x3619                     
36B9  3c 01                cmp al, 1                       
36BB  e0 f9                loopne 0x36b6                   
36BD  ba 13 00             mov dx, 0x13                    
36C0  e8 88 ed             call 0x244b                     
36C3  24 fd                and al, 0xfd                    
36C5  e8 aa ed             call 0x2472                     
36C8  ba 12 00             mov dx, 0x12                    
36CB  e8 7d ed             call 0x244b                     
36CE  ba 12 00             mov dx, 0x12                    
36D1  24 fc                and al, 0xfc                    
36D3  a2 f4 0b             mov byte ptr [0xbf4], al        
36D6  e8 99 ed             call 0x2472                     
36D9  59                   pop cx                          
36DA  58                   pop ax                          
36DB  5a                   pop dx                          
36DC  c3                   ret                             
36DD  b0 00                mov al, 0                       
36DF  c3                   ret                             
36E0  a0 5c 0c             mov al, byte ptr [0xc5c]        
36E3  c3                   ret                             
36E4  51                   push cx                         
36E5  56                   push si                         
36E6  57                   push di                         
36E7  fc                   cld                             
36E8  57                   push di                         
36E9  33 c0                xor ax, ax                      
36EB  f3 aa                rep stosb byte ptr es:[di], al  
36ED  5f                   pop di                          
36EE  b9 08 00             mov cx, 8                       
36F1  be 1f 0c             mov si, 0xc1f                      ; 0C1F='EPAT'
36F4  80 3e 5c 0c c4       cmp byte ptr [0xc5c], 0xc4      
36F9  76 0d                jbe 0x3708                      
36FB  be 27 0c             mov si, 0xc27                      ; 0C27='EPEZ'
36FE  80 3e 5c 0c c5       cmp byte ptr [0xc5c], 0xc5      
3703  74 03                je 0x3708                       
3705  be 2f 0c             mov si, 0xc2f                      ; 0C2F='EPAT+'
3708  f3 a4                rep movsb byte ptr es:[di], byte ptr [si]
370A  5f                   pop di                          
370B  5e                   pop si                          
370C  59                   pop cx                          
370D  c3                   ret                             
370E  a0 04 0c             mov al, byte ptr [0xc04]        
3711  c3                   ret                             
3712  51                   push cx                         
3713  8a 0e fc 0b          mov cl, byte ptr [0xbfc]        
3717  8a 2e 0b 0c          mov ch, byte ptr [0xc0b]        
371B  51                   push cx                         
371C  a2 fc 0b             mov byte ptr [0xbfc], al        
371F  80 26 fc 0b 0f       and byte ptr [0xbfc], 0xf       
3724  a2 0b 0c             mov byte ptr [0xc0b], al        
3727  e8 ba f7             call 0x2ee4                     
372A  e8 63 63             call 0x9a90                     
372D  e8 11 f9             call 0x3041                     
3730  a0 fc 0b             mov al, byte ptr [0xbfc]        
3733  59                   pop cx                          
3734  88 0e fc 0b          mov byte ptr [0xbfc], cl        
3738  88 2e 0b 0c          mov byte ptr [0xc0b], ch        
373C  3c ff                cmp al, 0xff                    
373E  b8 ff ff             mov ax, 0xffff                  
3741  74 03                je 0x3746                       
3743  b8 00 00             mov ax, 0                       
3746  59                   pop cx                          
3747  c3                   ret                             
3748  51                   push cx                         
3749  52                   push dx                         
374A  b9 08 00             mov cx, 8                       
374D  a0 04 0c             mov al, byte ptr [0xc04]        
3750  b4 01                mov ah, 1                       
3752  33 d2                xor dx, dx                      
3754  84 c4                test ah, al                     
3756  74 01                je 0x3759                       
3758  42                   inc dx                          
3759  d0 e4                shl ah, 1                       
375B  49                   dec cx                          
375C  75 f6                jne 0x3754                      
375E  33 c0                xor ax, ax                      
3760  8a c2                mov al, dl                      
3762  5a                   pop dx                          
3763  59                   pop cx                          
3764  c3                   ret                             
3765  a2 04 0c             mov byte ptr [0xc04], al        
3768  c3                   ret                             
3769  a2 ca 0b             mov byte ptr [0xbca], al        
376C  c3                   ret                             
376D  a0 ca 0b             mov al, byte ptr [0xbca]        
3770  c3                   ret                             
3771  53                   push bx                         
3772  52                   push dx                         
3773  80 3e 10 0c 01       cmp byte ptr [0xc10], 1         
3778  74 04                je 0x377e                       
377A  33 c0                xor ax, ax                      
377C  eb 79                jmp 0x37f7                      
377E  e8 e5 1c             call 0x5466                     
3781  3c 01                cmp al, 1                       
3783  b8 00 00             mov ax, 0                       
3786  75 6f                jne 0x37f7                      
3788  80 3e d7 0b 0a       cmp byte ptr [0xbd7], 0xa          ; 000A='SCSIMGR'
378D  72 68                jb 0x37f7                       
378F  80 3e d7 0b 0c       cmp byte ptr [0xbd7], 0xc          ; 000C='SIMGR'
3794  74 61                je 0x37f7                       
3796  b0 0b                mov al, 0xb                        ; 000B='CSIMGR'
3798  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
379C  ee                   out dx, al                      
379D  80 3e 15 0c 00       cmp byte ptr [0xc15], 0         
37A2  74 02                je 0x37a6                       
37A4  ee                   out dx, al                      
37A5  ee                   out dx, al                      
37A6  83 c2 02             add dx, 2                       
37A9  b0 04                mov al, 4                       
37AB  ee                   out dx, al                      
37AC  80 3e 15 0c 00       cmp byte ptr [0xc15], 0         
37B1  74 02                je 0x37b5                       
37B3  ee                   out dx, al                      
37B4  ee                   out dx, al                      
37B5  eb 00                jmp 0x37b7                      
37B7  eb 00                jmp 0x37b9                      
37B9  b0 0c                mov al, 0xc                        ; 000C='SIMGR'
37BB  ee                   out dx, al                      
37BC  ee                   out dx, al                      
37BD  80 3e 15 0c 00       cmp byte ptr [0xc15], 0         
37C2  74 02                je 0x37c6                       
37C4  ee                   out dx, al                      
37C5  ee                   out dx, al                      
37C6  eb 00                jmp 0x37c8                      
37C8  eb 00                jmp 0x37ca                      
37CA  b0 04                mov al, 4                       
37CC  ee                   out dx, al                      
37CD  80 3e 15 0c 00       cmp byte ptr [0xc15], 0         
37D2  74 02                je 0x37d6                       
37D4  ee                   out dx, al                      
37D5  ee                   out dx, al                      
37D6  b0 24                mov al, 0x24                    
37D8  ee                   out dx, al                      
37D9  ee                   out dx, al                      
37DA  b0 26                mov al, 0x26                    
37DC  ee                   out dx, al                      
37DD  ee                   out dx, al                      
37DE  83 ea 02             sub dx, 2                       
37E1  ec                   in al, dx                       
37E2  8a d8                mov bl, al                      
37E4  83 c2 02             add dx, 2                       
37E7  b0 04                mov al, 4                       
37E9  ee                   out dx, al                      
37EA  3a 1e 5c 0c          cmp bl, byte ptr [0xc5c]        
37EE  b8 ff ff             mov ax, 0xffff                  
37F1  75 16                jne 0x3809                      
37F3  33 c0                xor ax, ax                      
37F5  eb 12                jmp 0x3809                      
37F7  ba 0b 00             mov dx, 0xb                        ; 000B='CSIMGR'
37FA  e8 4e ec             call 0x244b                     
37FD  3a 06 5c 0c          cmp al, byte ptr [0xc5c]        
3801  b8 ff ff             mov ax, 0xffff                  
3804  75 03                jne 0x3809                      
3806  b8 00 00             mov ax, 0                       
3809  5a                   pop dx                          
380A  5b                   pop bx                          
380B  c3                   ret                             
380C  51                   push cx                         
380D  33 c9                xor cx, cx                      
380F  b2 0e                mov dl, 0xe                     
3811  b0 04                mov al, 4                       
3813  e8 5c ec             call 0x2472                     
3816  b2 0f                mov dl, 0xf                     
3818  e8 30 ec             call 0x244b                     
381B  8a e8                mov ch, al                      
381D  b2 0e                mov dl, 0xe                     
381F  b0 05                mov al, 5                       
3821  e8 4e ec             call 0x2472                     
3824  b2 0f                mov dl, 0xf                     
3826  e8 22 ec             call 0x244b                     
3829  24 03                and al, 3                       
382B  33 d2                xor dx, dx                      
382D  8a d0                mov dl, al                      
382F  d1 e2                shl dx, 1                       
3831  d1 e1                shl cx, 1                       
3833  73 03                jae 0x3838                      
3835  83 ca 01             or dx, 1                        
3838  8b c1                mov ax, cx                      
383A  59                   pop cx                          
383B  c3                   ret                             
383C  51                   push cx                         
383D  33 c9                xor cx, cx                      
383F  b2 0e                mov dl, 0xe                     
3841  b0 06                mov al, 6                       
3843  e8 2c ec             call 0x2472                     
3846  b2 0f                mov dl, 0xf                     
3848  e8 00 ec             call 0x244b                     
384B  8a e8                mov ch, al                      
384D  b2 0e                mov dl, 0xe                     
384F  b0 07                mov al, 7                       
3851  e8 1e ec             call 0x2472                     
3854  b2 0f                mov dl, 0xf                     
3856  e8 f2 eb             call 0x244b                     
3859  24 03                and al, 3                       
385B  33 d2                xor dx, dx                      
385D  8a d0                mov dl, al                      
385F  d1 e2                shl dx, 1                       
3861  d1 e1                shl cx, 1                       
3863  73 03                jae 0x3868                      
3865  83 ca 01             or dx, 1                        
3868  8b c1                mov ax, cx                      
386A  59                   pop cx                          
386B  c3                   ret                             
386C  52                   push dx                         
386D  51                   push cx                         
386E  b9 00 02             mov cx, 0x200                   
3871  f7 f1                div cx                          
3873  59                   pop cx                          
3874  ba 0e 00             mov dx, 0xe                     
3877  b0 06                mov al, 6                       
3879  e8 f6 eb             call 0x2472                     
387C  b2 0f                mov dl, 0xf                     
387E  58                   pop ax                          
387F  50                   push ax                         
3880  e8 ef eb             call 0x2472                     
3883  ba 0e 00             mov dx, 0xe                     
3886  b0 07                mov al, 7                       
3888  e8 e7 eb             call 0x2472                     
388B  b2 0f                mov dl, 0xf                     
388D  58                   pop ax                          
388E  8a c4                mov al, ah                      
3890  24 03                and al, 3                       
3892  e8 dd eb             call 0x2472                     
3895  5a                   pop dx                          
3896  c3                   ret                             
3897  52                   push dx                         
3898  51                   push cx                         
3899  b9 00 02             mov cx, 0x200                   
389C  f7 f1                div cx                          
389E  59                   pop cx                          
389F  50                   push ax                         
38A0  b2 0e                mov dl, 0xe                     
38A2  b0 04                mov al, 4                       
38A4  e8 cb eb             call 0x2472                     
38A7  b2 0f                mov dl, 0xf                     
38A9  58                   pop ax                          
38AA  50                   push ax                         
38AB  e8 c4 eb             call 0x2472                     
38AE  b2 0e                mov dl, 0xe                     
38B0  b0 05                mov al, 5                       
38B2  e8 bd eb             call 0x2472                     
38B5  b2 0f                mov dl, 0xf                     
38B7  58                   pop ax                          
38B8  8a c4                mov al, ah                      
38BA  24 03                and al, 3                       
38BC  e8 b3 eb             call 0x2472                     
38BF  5a                   pop dx                          
38C0  c3                   ret                             
38C1  53                   push bx                         
38C2  51                   push cx                         
38C3  83 f8 02             cmp ax, 2                       
38C6  bb ff ff             mov bx, 0xffff                  
38C9  77 2c                ja 0x38f7                       
38CB  b9 08 00             mov cx, 8                       
38CE  8a 1e 04 0c          mov bl, byte ptr [0xc04]        
38D2  d0 eb                shr bl, 1                       
38D4  72 02                jb 0x38d8                       
38D6  e2 fa                loop 0x38d2                     
38D8  0a db                or bl, bl                       
38DA  bb ff ff             mov bx, 0xffff                  
38DD  75 18                jne 0x38f7                      
38DF  c6 06 ca 0b ff       mov byte ptr [0xbca], 0xff      
38E4  c6 06 04 0c 00       mov byte ptr [0xc04], 0         
38E9  a2 8a 0c             mov byte ptr [0xc8a], al        
38EC  e8 f5 f5             call 0x2ee4                     
38EF  e8 1d 5b             call 0x940f                     
38F2  e8 4c f7             call 0x3041                     
38F5  33 db                xor bx, bx                      
38F7  33 c0                xor ax, ax                      
38F9  8b c3                mov ax, bx                      
38FB  83 e0 ff             and ax, 0xffff                  
38FE  59                   pop cx                          
38FF  5b                   pop bx                          
3900  c3                   ret                             
3901  55                   push bp                         
3902  4e                   dec si                          
3903  49                   dec cx                          
3904  44                   inc sp                          
3905  49                   dec cx                          
3906  52                   push dx                         
3907  20 46 61             and byte ptr [bp + 0x61], al    
390A  73 74                jae 0x3980                         ; 3980='IR Slow'
390C  00 00                add byte ptr [bx + si], al      
390E  00 00                add byte ptr [bx + si], al      
3910  00 00                add byte ptr [bx + si], al      
3912  00 00                add byte ptr [bx + si], al      
3914  00 0c                add byte ptr [si], cl           
3916  40                   inc ax                          
3917  ee                   out dx, al                      
3918  83 c2 02             add dx, 2                       
391B  b0 01                mov al, 1                       
391D  ee                   out dx, al                      
391E  ee                   out dx, al                      
391F  ee                   out dx, al                      
3920  ee                   out dx, al                      
3921  b0 04                mov al, 4                       
3923  ee                   out dx, al                      
3924  4a                   dec dx                          
3925  ec                   in al, dx                       
3926  8a e0                mov ah, al                      
3928  42                   inc dx                          
3929  ec                   in al, dx                       
392A  86 e0                xchg al, ah                     
392C  4a                   dec dx                          
392D  d0 ec                shr ah, 1                       
392F  d1 e8                shr ax, 1                       
3931  d1 e8                shr ax, 1                       
3933  d1 e8                shr ax, 1                       
3935  86 e0                xchg al, ah                     
3937  b0 ff                mov al, 0xff                    
3939  4a                   dec dx                          
393A  ee                   out dx, al                      
393B  86 e0                xchg al, ah                     
393D  c3                   ret                             
393E  55                   push bp                         
393F  4e                   dec si                          
3940  49                   dec cx                          
3941  44                   inc sp                          
3942  49                   dec cx                          
3943  52                   push dx                         
3944  20 4e 6f             and byte ptr [bp + 0x6f], cl    
3947  72 6d                jb 0x39b6                       
3949  61                   popaw                           
394A  6c                   insb byte ptr es:[di], dx       
394B  00 00                add byte ptr [bx + si], al      
394D  00 00                add byte ptr [bx + si], al      
394F  00 00                add byte ptr [bx + si], al      
3951  00 0c                add byte ptr [si], cl           
3953  40                   inc ax                          
3954  ee                   out dx, al                      
3955  83 c2 02             add dx, 2                       
3958  b0 01                mov al, 1                       
395A  ee                   out dx, al                      
395B  ee                   out dx, al                      
395C  ee                   out dx, al                      
395D  ee                   out dx, al                      
395E  b0 04                mov al, 4                       
3960  ee                   out dx, al                      
3961  ee                   out dx, al                      
3962  4a                   dec dx                          
3963  ec                   in al, dx                       
3964  8a e0                mov ah, al                      
3966  42                   inc dx                          
3967  ec                   in al, dx                       
3968  86 e0                xchg al, ah                     
396A  4a                   dec dx                          
396B  d0 ec                shr ah, 1                       
396D  d1 e8                shr ax, 1                       
396F  d1 e8                shr ax, 1                       
3971  d1 e8                shr ax, 1                       
3973  b4 ff                mov ah, 0xff                    
3975  86 e0                xchg al, ah                     
3977  4a                   dec dx                          
3978  ee                   out dx, al                      
3979  86 e0                xchg al, ah                     
397B  c3                   ret                             
397C  55                   push bp                         
397D  4e                   dec si                          
397E  49                   dec cx                          
397F  44                   inc sp                          
3980  49                   dec cx                          
3981  52                   push dx                         
3982  20 53 6c             and byte ptr [bp + di + 0x6c], dl
3985  6f                   outsw dx, word ptr [si]         
3986  77 00                ja 0x3988                       
3988  00 00                add byte ptr [bx + si], al      
398A  00 00                add byte ptr [bx + si], al      
398C  00 00                add byte ptr [bx + si], al      
398E  00 00                add byte ptr [bx + si], al      
3990  0c 40                or al, 0x40                     
3992  ee                   out dx, al                      
3993  ee                   out dx, al                      
3994  83 c2 02             add dx, 2                       
3997  b0 01                mov al, 1                       
3999  ee                   out dx, al                      
399A  ee                   out dx, al                      
399B  ee                   out dx, al                      
399C  ee                   out dx, al                      
399D  b0 04                mov al, 4                       
399F  ee                   out dx, al                      
39A0  ee                   out dx, al                      
39A1  ee                   out dx, al                      
39A2  ee                   out dx, al                      
39A3  4a                   dec dx                          
39A4  ec                   in al, dx                       
39A5  ec                   in al, dx                       
39A6  8a e0                mov ah, al                      
39A8  42                   inc dx                          
39A9  ec                   in al, dx                       
39AA  ec                   in al, dx                       
39AB  86 e0                xchg al, ah                     
39AD  4a                   dec dx                          
39AE  d0 ec                shr ah, 1                       
39B0  d1 e8                shr ax, 1                       
39B2  d1 e8                shr ax, 1                       
39B4  d1 e8                shr ax, 1                       
39B6  86 e0                xchg al, ah                     
39B8  4a                   dec dx                          
39B9  b0 ff                mov al, 0xff                    
39BB  ee                   out dx, al                      
39BC  ee                   out dx, al                      
39BD  86 e0                xchg al, ah                     
39BF  c3                   ret                             
39C0  55                   push bp                         
39C1  4e                   dec si                          
39C2  49                   dec cx                          
39C3  44                   inc sp                          
39C4  49                   dec cx                          
39C5  52                   push dx                         
39C6  20 74 77             and byte ptr [si + 0x77], dh    
39C9  6f                   outsw dx, word ptr [si]         
39CA  20 77 61             and byte ptr [bx + 0x61], dh    
39CD  69 74 00 00 00       imul si, word ptr [si], 0       
39D2  00 00                add byte ptr [bx + si], al      
39D4  0c 40                or al, 0x40                     
39D6  ee                   out dx, al                      
39D7  ee                   out dx, al                      
39D8  ee                   out dx, al                      
39D9  83 c2 02             add dx, 2                       
39DC  b0 01                mov al, 1                       
39DE  ee                   out dx, al                      
39DF  ee                   out dx, al                      
39E0  ee                   out dx, al                      
39E1  ee                   out dx, al                      
39E2  ee                   out dx, al                      
39E3  b0 04                mov al, 4                       
39E5  ee                   out dx, al                      
39E6  ee                   out dx, al                      
39E7  ee                   out dx, al                      
39E8  ee                   out dx, al                      
39E9  ee                   out dx, al                      
39EA  4a                   dec dx                          
39EB  ec                   in al, dx                       
39EC  ec                   in al, dx                       
39ED  8a e0                mov ah, al                      
39EF  42                   inc dx                          
39F0  ec                   in al, dx                       
39F1  ec                   in al, dx                       
39F2  86 e0                xchg al, ah                     
39F4  4a                   dec dx                          
39F5  d0 ec                shr ah, 1                       
39F7  d1 e8                shr ax, 1                       
39F9  d1 e8                shr ax, 1                       
39FB  d1 e8                shr ax, 1                       
39FD  86 e0                xchg al, ah                     
39FF  4a                   dec dx                          
3A00  b0 ff                mov al, 0xff                    
3A02  ee                   out dx, al                      
3A03  ee                   out dx, al                      
3A04  86 e0                xchg al, ah                     
3A06  c3                   ret                             
3A07  4e                   dec si                          
3A08  49                   dec cx                          
3A09  42                   inc dx                          
3A0A  42                   inc dx                          
3A0B  4c                   dec sp                          
3A0C  45                   inc bp                          
3A0D  20 46 61             and byte ptr [bp + 0x61], al    
3A10  73 74                jae 0x3a86                      
3A12  00 00                add byte ptr [bx + si], al      
3A14  00 00                add byte ptr [bx + si], al      
3A16  00 00                add byte ptr [bx + si], al      
3A18  00 00                add byte ptr [bx + si], al      
3A1A  00 0c                add byte ptr [si], cl           
3A1C  00 ee                add dh, ch                      
3A1E  83 c2 02             add dx, 2                       
3A21  b0 01                mov al, 1                       
3A23  ee                   out dx, al                      
3A24  b0 03                mov al, 3                       
3A26  ee                   out dx, al                      
3A27  ee                   out dx, al                      
3A28  4a                   dec dx                          
3A29  ec                   in al, dx                       
3A2A  8a e0                mov ah, al                      
3A2C  42                   inc dx                          
3A2D  b0 04                mov al, 4                       
3A2F  ee                   out dx, al                      
3A30  ee                   out dx, al                      
3A31  4a                   dec dx                          
3A32  ec                   in al, dx                       
3A33  25 f0 f0             and ax, 0xf0f0                  
3A36  d0 ec                shr ah, 1                       
3A38  d0 ec                shr ah, 1                       
3A3A  d0 ec                shr ah, 1                       
3A3C  d0 ec                shr ah, 1                       
3A3E  0a c4                or al, ah                       
3A40  c3                   ret                             
3A41  4e                   dec si                          
3A42  49                   dec cx                          
3A43  42                   inc dx                          
3A44  42                   inc dx                          
3A45  4c                   dec sp                          
3A46  45                   inc bp                          
3A47  20 4e 6f             and byte ptr [bp + 0x6f], cl    
3A4A  72 6d                jb 0x3ab9                       
3A4C  61                   popaw                           
3A4D  6c                   insb byte ptr es:[di], dx       
3A4E  00 00                add byte ptr [bx + si], al      
3A50  00 00                add byte ptr [bx + si], al      
3A52  00 00                add byte ptr [bx + si], al      
3A54  00 0c                add byte ptr [si], cl           
3A56  00 ee                add dh, ch                      
3A58  83 c2 02             add dx, 2                       
3A5B  b0 01                mov al, 1                       
3A5D  ee                   out dx, al                      
3A5E  b0 03                mov al, 3                       
3A60  ee                   out dx, al                      
3A61  ee                   out dx, al                      
3A62  ee                   out dx, al                      
3A63  4a                   dec dx                          
3A64  ec                   in al, dx                       
3A65  8a e0                mov ah, al                      
3A67  42                   inc dx                          
3A68  b0 04                mov al, 4                       
3A6A  ee                   out dx, al                      
3A6B  ee                   out dx, al                      
3A6C  4a                   dec dx                          
3A6D  ec                   in al, dx                       
3A6E  25 f0 f0             and ax, 0xf0f0                  
3A71  d0 ec                shr ah, 1                       
3A73  d0 ec                shr ah, 1                       
3A75  d0 ec                shr ah, 1                       
3A77  d0 ec                shr ah, 1                       
3A79  0a c4                or al, ah                       
3A7B  c3                   ret                             
3A7C  4e                   dec si                          
3A7D  49                   dec cx                          
3A7E  42                   inc dx                          
3A7F  42                   inc dx                          
3A80  4c                   dec sp                          
3A81  45                   inc bp                          
3A82  20 53 6c             and byte ptr [bp + di + 0x6c], dl
3A85  6f                   outsw dx, word ptr [si]         
3A86  77 00                ja 0x3a88                       
3A88  00 00                add byte ptr [bx + si], al      
3A8A  00 00                add byte ptr [bx + si], al      
3A8C  00 00                add byte ptr [bx + si], al      
3A8E  00 00                add byte ptr [bx + si], al      
3A90  0c 00                or al, 0                        
3A92  ee                   out dx, al                      
3A93  ee                   out dx, al                      
3A94  83 c2 02             add dx, 2                       
3A97  b0 01                mov al, 1                       
3A99  ee                   out dx, al                      
3A9A  b0 03                mov al, 3                       
3A9C  ee                   out dx, al                      
3A9D  ee                   out dx, al                      
3A9E  ee                   out dx, al                      
3A9F  ee                   out dx, al                      
3AA0  4a                   dec dx                          
3AA1  ec                   in al, dx                       
3AA2  ec                   in al, dx                       
3AA3  8a e0                mov ah, al                      
3AA5  42                   inc dx                          
3AA6  b0 04                mov al, 4                       
3AA8  ee                   out dx, al                      
3AA9  ee                   out dx, al                      
3AAA  ee                   out dx, al                      
3AAB  4a                   dec dx                          
3AAC  ec                   in al, dx                       
3AAD  ec                   in al, dx                       
3AAE  25 f0 f0             and ax, 0xf0f0                  
3AB1  d0 ec                shr ah, 1                       
3AB3  d0 ec                shr ah, 1                       
3AB5  d0 ec                shr ah, 1                       
3AB7  d0 ec                shr ah, 1                       
3AB9  0a c4                or al, ah                       
3ABB  c3                   ret                             
3ABC  4e                   dec si                          
3ABD  49                   dec cx                          
3ABE  42                   inc dx                          
3ABF  42                   inc dx                          
3AC0  4c                   dec sp                          
3AC1  45                   inc bp                          
3AC2  20 53 6c             and byte ptr [bp + di + 0x6c], dl
3AC5  6f                   outsw dx, word ptr [si]         
3AC6  77 28                ja 0x3af0                       
3AC8  2d 29 00             sub ax, 0x29                    
3ACB  00 00                add byte ptr [bx + si], al      
3ACD  00 00                add byte ptr [bx + si], al      
3ACF  00 0c                add byte ptr [si], cl           
3AD1  00 ee                add dh, ch                      
3AD3  ee                   out dx, al                      
3AD4  ee                   out dx, al                      
3AD5  ee                   out dx, al                      
3AD6  83 c2 02             add dx, 2                       
3AD9  b0 01                mov al, 1                       
3ADB  ee                   out dx, al                      
3ADC  ee                   out dx, al                      
3ADD  b0 03                mov al, 3                       
3ADF  ee                   out dx, al                      
3AE0  ee                   out dx, al                      
3AE1  ee                   out dx, al                      
3AE2  ee                   out dx, al                      
3AE3  ee                   out dx, al                      
3AE4  4a                   dec dx                          
3AE5  ec                   in al, dx                       
3AE6  ec                   in al, dx                       
3AE7  ec                   in al, dx                       
3AE8  8a e0                mov ah, al                      
3AEA  42                   inc dx                          
3AEB  b0 04                mov al, 4                       
3AED  ee                   out dx, al                      
3AEE  ee                   out dx, al                      
3AEF  ee                   out dx, al                      
3AF0  ee                   out dx, al                      
3AF1  4a                   dec dx                          
3AF2  ec                   in al, dx                       
3AF3  ec                   in al, dx                       
3AF4  ec                   in al, dx                       
3AF5  25 f0 f0             and ax, 0xf0f0                  
3AF8  d0 ec                shr ah, 1                       
3AFA  d0 ec                shr ah, 1                       
3AFC  d0 ec                shr ah, 1                       
3AFE  d0 ec                shr ah, 1                       
3B00  0a c4                or al, ah                       
3B02  c3                   ret                             
3B03  54                   push sp                         
3B04  4f                   dec di                          
3B05  53                   push bx                         
3B06  48                   dec ax                          
3B07  49                   dec cx                          
3B08  42                   inc dx                          
3B09  41                   inc cx                          
3B0A  20 46 61             and byte ptr [bp + 0x61], al    
3B0D  73 74                jae 0x3b83                         ; 3B83=' Fast'
3B0F  00 00                add byte ptr [bx + si], al      
3B11  00 00                add byte ptr [bx + si], al      
3B13  00 00                add byte ptr [bx + si], al      
3B15  00 00                add byte ptr [bx + si], al      
3B17  0c 20                or al, 0x20                     
3B19  ee                   out dx, al                      
3B1A  83 c2 02             add dx, 2                       
3B1D  b0 01                mov al, 1                       
3B1F  ee                   out dx, al                      
3B20  ee                   out dx, al                      
3B21  83 c2 05             add dx, 5                       
3B24  b0 01                mov al, 1                       
3B26  ee                   out dx, al                      
3B27  83 ea 05             sub dx, 5                       
3B2A  b0 85                mov al, 0x85                    
3B2C  ee                   out dx, al                      
3B2D  ee                   out dx, al                      
3B2E  ee                   out dx, al                      
3B2F  83 ea 02             sub dx, 2                       
3B32  ec                   in al, dx                       
3B33  83 c2 02             add dx, 2                       
3B36  b4 04                mov ah, 4                       
3B38  86 e0                xchg al, ah                     
3B3A  ee                   out dx, al                      
3B3B  86 e0                xchg al, ah                     
3B3D  c3                   ret                             
3B3E  54                   push sp                         
3B3F  4f                   dec di                          
3B40  53                   push bx                         
3B41  48                   dec ax                          
3B42  49                   dec cx                          
3B43  42                   inc dx                          
3B44  41                   inc cx                          
3B45  20 4e 6f             and byte ptr [bp + 0x6f], cl    
3B48  72 6d                jb 0x3bb7                          ; 3BB7='ormal'
3B4A  61                   popaw                           
3B4B  6c                   insb byte ptr es:[di], dx       
3B4C  00 00                add byte ptr [bx + si], al      
3B4E  00 00                add byte ptr [bx + si], al      
3B50  00 00                add byte ptr [bx + si], al      
3B52  0c 20                or al, 0x20                     
3B54  ee                   out dx, al                      
3B55  ee                   out dx, al                      
3B56  83 c2 02             add dx, 2                       
3B59  b0 01                mov al, 1                       
3B5B  ee                   out dx, al                      
3B5C  ee                   out dx, al                      
3B5D  ee                   out dx, al                      
3B5E  83 c2 05             add dx, 5                       
3B61  b0 01                mov al, 1                       
3B63  ee                   out dx, al                      
3B64  ee                   out dx, al                      
3B65  83 ea 05             sub dx, 5                       
3B68  b0 85                mov al, 0x85                    
3B6A  ee                   out dx, al                      
3B6B  ee                   out dx, al                      
3B6C  ee                   out dx, al                      
3B6D  ee                   out dx, al                      
3B6E  83 ea 02             sub dx, 2                       
3B71  ec                   in al, dx                       
3B72  ec                   in al, dx                       
3B73  83 c2 02             add dx, 2                       
3B76  b4 04                mov ah, 4                       
3B78  86 e0                xchg al, ah                     
3B7A  ee                   out dx, al                      
3B7B  ee                   out dx, al                      
3B7C  86 e0                xchg al, ah                     
3B7E  c3                   ret                             
3B7F  50                   push ax                         
3B80  53                   push bx                         
3B81  2f                   das                             
3B82  32 20                xor ah, byte ptr [bx + si]      
3B84  46                   inc si                          
3B85  61                   popaw                           
3B86  73 74                jae 0x3bfc                      
3B88  00 00                add byte ptr [bx + si], al      
3B8A  00 00                add byte ptr [bx + si], al      
3B8C  00 00                add byte ptr [bx + si], al      
3B8E  00 00                add byte ptr [bx + si], al      
3B90  00 00                add byte ptr [bx + si], al      
3B92  00 0c                add byte ptr [si], cl           
3B94  20 ee                and dh, ch                      
3B96  83 c2 02             add dx, 2                       
3B99  b0 01                mov al, 1                       
3B9B  ee                   out dx, al                      
3B9C  ee                   out dx, al                      
3B9D  b0 25                mov al, 0x25                    
3B9F  ee                   out dx, al                      
3BA0  ee                   out dx, al                      
3BA1  ee                   out dx, al                      
3BA2  83 ea 02             sub dx, 2                       
3BA5  ec                   in al, dx                       
3BA6  83 c2 02             add dx, 2                       
3BA9  b4 04                mov ah, 4                       
3BAB  86 e0                xchg al, ah                     
3BAD  ee                   out dx, al                      
3BAE  86 e0                xchg al, ah                     
3BB0  c3                   ret                             
3BB1  50                   push ax                         
3BB2  53                   push bx                         
3BB3  2f                   das                             
3BB4  32 20                xor ah, byte ptr [bx + si]      
3BB6  4e                   dec si                          
3BB7  6f                   outsw dx, word ptr [si]         
3BB8  72 6d                jb 0x3c27                       
3BBA  61                   popaw                           
3BBB  6c                   insb byte ptr es:[di], dx       
3BBC  00 00                add byte ptr [bx + si], al      
3BBE  00 00                add byte ptr [bx + si], al      
3BC0  00 00                add byte ptr [bx + si], al      
3BC2  00 00                add byte ptr [bx + si], al      
3BC4  00 0c                add byte ptr [si], cl           
3BC6  20 ee                and dh, ch                      
3BC8  ee                   out dx, al                      
3BC9  83 c2 02             add dx, 2                       
3BCC  b0 01                mov al, 1                       
3BCE  ee                   out dx, al                      
3BCF  ee                   out dx, al                      
3BD0  ee                   out dx, al                      
3BD1  b0 25                mov al, 0x25                    
3BD3  ee                   out dx, al                      
3BD4  ee                   out dx, al                      
3BD5  ee                   out dx, al                      
3BD6  ee                   out dx, al                      
3BD7  83 ea 02             sub dx, 2                       
3BDA  ec                   in al, dx                       
3BDB  ec                   in al, dx                       
3BDC  83 c2 02             add dx, 2                       
3BDF  b4 04                mov ah, 4                       
3BE1  86 e0                xchg al, ah                     
3BE3  ee                   out dx, al                      
3BE4  ee                   out dx, al                      
3BE5  86 e0                xchg al, ah                     
3BE7  c3                   ret                             
3BE8  45                   inc bp                          
3BE9  50                   push ax                         
3BEA  50                   push ax                         
3BEB  20 42 49             and byte ptr [bp + si + 0x49], al
3BEE  4f                   dec di                          
3BEF  53                   push bx                         
3BF0  28 46 29             sub byte ptr [bp + 0x29], al    
3BF3  00 45 50             add byte ptr [di + 0x50], al    
3BF6  50                   push ax                         
3BF7  20 46 61             and byte ptr [bp + 0x61], al    
3BFA  73 74                jae 0x3c70                      
3BFC  00 45 50             add byte ptr [di + 0x50], al    
3BFF  50                   push ax                         
3C00  20 42 49             and byte ptr [bp + si + 0x49], al
3C03  4f                   dec di                          
3C04  53                   push bx                         
3C05  28 46 29             sub byte ptr [bp + 0x29], al    
3C08  00 45 50             add byte ptr [di + 0x50], al    
3C0B  50                   push ax                         
3C0C  20 46 61             and byte ptr [bp + 0x61], al    
3C0F  73 74                jae 0x3c85                      
3C11  00 45 50             add byte ptr [di + 0x50], al    
3C14  50                   push ax                         
3C15  20 4e 6f             and byte ptr [bp + 0x6f], cl    
3C18  72 6d                jb 0x3c87                       
3C1A  61                   popaw                           
3C1B  6c                   insb byte ptr es:[di], dx       
3C1C  00 00                add byte ptr [bx + si], al      
3C1E  00 00                add byte ptr [bx + si], al      
3C20  00 00                add byte ptr [bx + si], al      
3C22  00 00                add byte ptr [bx + si], al      
3C24  00 00                add byte ptr [bx + si], al      
3C26  80 3e c7 0b 01       cmp byte ptr [0xbc7], 1         
3C2B  75 14                jne 0x3c41                      
3C2D  83 c2 03             add dx, 3                       
3C30  50                   push ax                         
3C31  ec                   in al, dx                       
3C32  24 f8                and al, 0xf8                    
3C34  0c 06                or al, 6                        
3C36  ee                   out dx, al                      
3C37  58                   pop ax                          
3C38  83 c2 03             add dx, 3                       
3C3B  ee                   out dx, al                      
3C3C  83 ea 02             sub dx, 2                       
3C3F  eb 41                jmp 0x3c82                      
3C41  80 3e c0 0b 03       cmp byte ptr [0xbc0], 3         
3C46  75 19                jne 0x3c61                      
3C48  50                   push ax                         
3C49  b8 00 80             mov ax, 0x8000                  
3C4C  e6 23                out 0x23, al                    
3C4E  eb 00                jmp 0x3c50                      
3C50  86 e0                xchg al, ah                     
3C52  e6 22                out 0x22, al                    
3C54  eb 00                jmp 0x3c56                      
3C56  e7 22                out 0x22, ax                    
3C58  e4 22                in al, 0x22                     
3C5A  24 1f                and al, 0x1f                    
3C5C  0c 21                or al, 0x21                     
3C5E  e6 22                out 0x22, al                    
3C60  58                   pop ax                          
3C61  83 c2 03             add dx, 3                       
3C64  ee                   out dx, al                      
3C65  80 3e c0 0b 03       cmp byte ptr [0xbc0], 3         
3C6A  75 15                jne 0x3c81                      
3C6C  50                   push ax                         
3C6D  b8 00 80             mov ax, 0x8000                  
3C70  e6 23                out 0x23, al                    
3C72  86 e0                xchg al, ah                     
3C74  e6 22                out 0x22, al                    
3C76  e7 22                out 0x22, ax                    
3C78  e4 22                in al, 0x22                     
3C7A  24 1f                and al, 0x1f                    
3C7C  0c 01                or al, 1                        
3C7E  e6 22                out 0x22, al                    
3C80  58                   pop ax                          
3C81  42                   inc dx                          
3C82  83 ea 02             sub dx, 2                       
3C85  ec                   in al, dx                       
3C86  24 1f                and al, 0x1f                    
3C88  0c 20                or al, 0x20                     
3C8A  ee                   out dx, al                      
3C8B  83 c2 02             add dx, 2                       
3C8E  ec                   in al, dx                       
3C8F  86 e0                xchg al, ah                     
3C91  83 ea 02             sub dx, 2                       
3C94  ec                   in al, dx                       
3C95  24 1f                and al, 0x1f                    
3C97  ee                   out dx, al                      
3C98  86 e0                xchg al, ah                     
3C9A  c3                   ret                             
3C9B  45                   inc bp                          
3C9C  50                   push ax                         
3C9D  50                   push ax                         
3C9E  20 42 49             and byte ptr [bp + si + 0x49], al
3CA1  4f                   dec di                          
3CA2  53                   push bx                         
3CA3  28 4e 29             sub byte ptr [bp + 0x29], cl    
3CA6  00 00                add byte ptr [bx + si], al      
3CA8  00 00                add byte ptr [bx + si], al      
3CAA  00 00                add byte ptr [bx + si], al      
3CAC  00 00                add byte ptr [bx + si], al      
3CAE  00 8a 16 c9          add byte ptr [bp + si - 0x36ea], cl
3CB2  0b b4 0b ff          or si, word ptr [si - 0xf5]     
3CB6  1e                   push ds                         
3CB7  e1 0b                loope 0x3cc4                    
3CB9  c3                   ret                             
3CBA  45                   inc bp                          
3CBB  43                   inc bx                          
3CBC  50                   push ax                         
3CBD  20 52 65             and byte ptr [bp + si + 0x65], dl
3CC0  61                   popaw                           
3CC1  64 00 00             add byte ptr fs:[bx + si], al   
3CC4  00 00                add byte ptr [bx + si], al      
3CC6  00 00                add byte ptr [bx + si], al      
3CC8  00 00                add byte ptr [bx + si], al      
3CCA  00 00                add byte ptr [bx + si], al      
3CCC  00 00                add byte ptr [bx + si], al      
3CCE  8a e0                mov ah, al                      
3CD0  b0 04                mov al, 4                       
3CD2  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
3CD6  83 c2 02             add dx, 2                       
3CD9  ee                   out dx, al                      
3CDA  81 c2 00 04          add dx, 0x400                   
3CDE  b0 74                mov al, 0x74                    
3CE0  ee                   out dx, al                      
3CE1  51                   push cx                         
3CE2  b9 ff ff             mov cx, 0xffff                  
3CE5  ec                   in al, dx                       
3CE6  a8 01                test al, 1                      
3CE8  e1 fb                loope 0x3ce5                    
3CEA  0b c9                or cx, cx                       
3CEC  59                   pop cx                          
3CED  75 04                jne 0x3cf3                      
3CEF  b4 ff                mov ah, 0xff                    
3CF1  eb 60                jmp 0x3d53                      
3CF3  81 ea 02 04          sub dx, 0x402                   
3CF7  8a c4                mov al, ah                      
3CF9  ee                   out dx, al                      
3CFA  81 c2 02 04          add dx, 0x402                   
3CFE  51                   push cx                         
3CFF  b9 ff ff             mov cx, 0xffff                  
3D02  ec                   in al, dx                       
3D03  a8 01                test al, 1                      
3D05  e1 fb                loope 0x3d02                    
3D07  0b c9                or cx, cx                       
3D09  b4 ff                mov ah, 0xff                    
3D0B  59                   pop cx                          
3D0C  75 02                jne 0x3d10                      
3D0E  eb 43                jmp 0x3d53                      
3D10  b0 34                mov al, 0x34                    
3D12  ee                   out dx, al                      
3D13  81 ea 00 04          sub dx, 0x400                   
3D17  ec                   in al, dx                       
3D18  b0 20                mov al, 0x20                    
3D1A  ee                   out dx, al                      
3D1B  81 c2 00 04          add dx, 0x400                   
3D1F  b0 74                mov al, 0x74                    
3D21  ee                   out dx, al                      
3D22  51                   push cx                         
3D23  b9 00 80             mov cx, 0x8000                  
3D26  ec                   in al, dx                       
3D27  a8 01                test al, 1                      
3D29  e0 fb                loopne 0x3d26                   
3D2B  0b c9                or cx, cx                       
3D2D  b4 ff                mov ah, 0xff                    
3D2F  59                   pop cx                          
3D30  75 0d                jne 0x3d3f                      
3D32  81 ea 00 04          sub dx, 0x400                   
3D36  b0 04                mov al, 4                       
3D38  ee                   out dx, al                      
3D39  81 c2 00 04          add dx, 0x400                   
3D3D  eb 14                jmp 0x3d53                      
3D3F  83 ea 02             sub dx, 2                       
3D42  ec                   in al, dx                       
3D43  8a e0                mov ah, al                      
3D45  81 ea fe 03          sub dx, 0x3fe                   
3D49  ec                   in al, dx                       
3D4A  24 10                and al, 0x10                    
3D4C  0c 04                or al, 4                        
3D4E  ee                   out dx, al                      
3D4F  81 c2 00 04          add dx, 0x400                   
3D53  b0 34                mov al, 0x34                    
3D55  ee                   out dx, al                      
3D56  8a c4                mov al, ah                      
3D58  c3                   ret                             
3D59  b0 47                mov al, 0x47                    
3D5B  ee                   out dx, al                      
3D5C  83 c2 02             add dx, 2                       
3D5F  b0 01                mov al, 1                       
3D61  ee                   out dx, al                      
3D62  ee                   out dx, al                      
3D63  ee                   out dx, al                      
3D64  ee                   out dx, al                      
3D65  b0 05                mov al, 5                       
3D67  ee                   out dx, al                      
3D68  b0 ff                mov al, 0xff                    
3D6A  83 ea 02             sub dx, 2                       
3D6D  ee                   out dx, al                      
3D6E  ee                   out dx, al                      
3D6F  42                   inc dx                          
3D70  42                   inc dx                          
3D71  83 e9 02             sub cx, 2                       
3D74  bb 05 04             mov bx, 0x405                   
3D77  d1 e9                shr cx, 1                       
3D79  8a c7                mov al, bh                      
3D7B  ee                   out dx, al                      
3D7C  ee                   out dx, al                      
3D7D  4a                   dec dx                          
3D7E  0b c9                or cx, cx                       
3D80  74 29                je 0x3dab                       
3D82  ec                   in al, dx                       
3D83  8a e0                mov ah, al                      
3D85  42                   inc dx                          
3D86  ec                   in al, dx                       
3D87  86 c3                xchg bl, al                     
3D89  ee                   out dx, al                      
3D8A  86 c3                xchg bl, al                     
3D8C  86 e0                xchg al, ah                     
3D8E  d0 ec                shr ah, 1                       
3D90  c1 e8 03             shr ax, 3                       
3D93  aa                   stosb byte ptr es:[di], al      
3D94  4a                   dec dx                          
3D95  ec                   in al, dx                       
3D96  8a e0                mov ah, al                      
3D98  42                   inc dx                          
3D99  ec                   in al, dx                       
3D9A  86 c7                xchg bh, al                     
3D9C  ee                   out dx, al                      
3D9D  86 c7                xchg bh, al                     
3D9F  86 e0                xchg al, ah                     
3DA1  d0 ec                shr ah, 1                       
3DA3  c1 e8 03             shr ax, 3                       
3DA6  aa                   stosb byte ptr es:[di], al      
3DA7  4a                   dec dx                          
3DA8  49                   dec cx                          
3DA9  75 36                jne 0x3de1                      
3DAB  ec                   in al, dx                       
3DAC  8a e0                mov ah, al                      
3DAE  42                   inc dx                          
3DAF  ec                   in al, dx                       
3DB0  86 c4                xchg ah, al                     
3DB2  d0 ec                shr ah, 1                       
3DB4  c1 e8 03             shr ax, 3                       
3DB7  aa                   stosb byte ptr es:[di], al      
3DB8  4a                   dec dx                          
3DB9  4a                   dec dx                          
3DBA  b0 fd                mov al, 0xfd                    
3DBC  ee                   out dx, al                      
3DBD  ee                   out dx, al                      
3DBE  42                   inc dx                          
3DBF  42                   inc dx                          
3DC0  8a c3                mov al, bl                      
3DC2  ee                   out dx, al                      
3DC3  ee                   out dx, al                      
3DC4  4a                   dec dx                          
3DC5  ec                   in al, dx                       
3DC6  8a e0                mov ah, al                      
3DC8  42                   inc dx                          
3DC9  ec                   in al, dx                       
3DCA  86 e0                xchg al, ah                     
3DCC  4a                   dec dx                          
3DCD  d0 ec                shr ah, 1                       
3DCF  d1 e8                shr ax, 1                       
3DD1  d1 e8                shr ax, 1                       
3DD3  d1 e8                shr ax, 1                       
3DD5  aa                   stosb byte ptr es:[di], al      
3DD6  4a                   dec dx                          
3DD7  b0 00                mov al, 0                       
3DD9  ee                   out dx, al                      
3DDA  83 c2 02             add dx, 2                       
3DDD  b0 04                mov al, 4                       
3DDF  ee                   out dx, al                      
3DE0  c3                   ret                             
3DE1  eb 9f                jmp 0x3d82                      
3DE3  b0 47                mov al, 0x47                    
3DE5  ee                   out dx, al                      
3DE6  83 c2 02             add dx, 2                       
3DE9  b0 01                mov al, 1                       
3DEB  ee                   out dx, al                      
3DEC  ee                   out dx, al                      
3DED  ee                   out dx, al                      
3DEE  ee                   out dx, al                      
3DEF  b0 05                mov al, 5                       
3DF1  ee                   out dx, al                      
3DF2  b0 ff                mov al, 0xff                    
3DF4  83 ea 02             sub dx, 2                       
3DF7  ee                   out dx, al                      
3DF8  ee                   out dx, al                      
3DF9  42                   inc dx                          
3DFA  42                   inc dx                          
3DFB  83 e9 02             sub cx, 2                       
3DFE  bb 04 05             mov bx, 0x504                   
3E01  d1 e9                shr cx, 1                       
3E03  0b c9                or cx, cx                       
3E05  74 2d                je 0x3e34                       
3E07  8a c3                mov al, bl                      
3E09  ee                   out dx, al                      
3E0A  ee                   out dx, al                      
3E0B  4a                   dec dx                          
3E0C  ec                   in al, dx                       
3E0D  8a e0                mov ah, al                      
3E0F  42                   inc dx                          
3E10  ec                   in al, dx                       
3E11  86 e0                xchg al, ah                     
3E13  d0 ec                shr ah, 1                       
3E15  d1 e8                shr ax, 1                       
3E17  d1 e8                shr ax, 1                       
3E19  d1 e8                shr ax, 1                       
3E1B  aa                   stosb byte ptr es:[di], al      
3E1C  8a c7                mov al, bh                      
3E1E  ee                   out dx, al                      
3E1F  ee                   out dx, al                      
3E20  4a                   dec dx                          
3E21  ec                   in al, dx                       
3E22  8a e0                mov ah, al                      
3E24  42                   inc dx                          
3E25  ec                   in al, dx                       
3E26  86 e0                xchg al, ah                     
3E28  d0 ec                shr ah, 1                       
3E2A  d1 e8                shr ax, 1                       
3E2C  d1 e8                shr ax, 1                       
3E2E  d1 e8                shr ax, 1                       
3E30  aa                   stosb byte ptr es:[di], al      
3E31  49                   dec cx                          
3E32  75 3e                jne 0x3e72                      
3E34  8a c3                mov al, bl                      
3E36  ee                   out dx, al                      
3E37  ee                   out dx, al                      
3E38  4a                   dec dx                          
3E39  ec                   in al, dx                       
3E3A  8a e0                mov ah, al                      
3E3C  42                   inc dx                          
3E3D  ec                   in al, dx                       
3E3E  86 e0                xchg al, ah                     
3E40  d0 ec                shr ah, 1                       
3E42  d1 e8                shr ax, 1                       
3E44  d1 e8                shr ax, 1                       
3E46  d1 e8                shr ax, 1                       
3E48  aa                   stosb byte ptr es:[di], al      
3E49  4a                   dec dx                          
3E4A  4a                   dec dx                          
3E4B  b0 fd                mov al, 0xfd                    
3E4D  ee                   out dx, al                      
3E4E  ee                   out dx, al                      
3E4F  42                   inc dx                          
3E50  42                   inc dx                          
3E51  8a c7                mov al, bh                      
3E53  ee                   out dx, al                      
3E54  ee                   out dx, al                      
3E55  4a                   dec dx                          
3E56  ec                   in al, dx                       
3E57  8a e0                mov ah, al                      
3E59  42                   inc dx                          
3E5A  ec                   in al, dx                       
3E5B  86 e0                xchg al, ah                     
3E5D  4a                   dec dx                          
3E5E  d0 ec                shr ah, 1                       
3E60  d1 e8                shr ax, 1                       
3E62  d1 e8                shr ax, 1                       
3E64  d1 e8                shr ax, 1                       
3E66  aa                   stosb byte ptr es:[di], al      
3E67  4a                   dec dx                          
3E68  b0 00                mov al, 0                       
3E6A  ee                   out dx, al                      
3E6B  83 c2 02             add dx, 2                       
3E6E  b0 04                mov al, 4                       
3E70  ee                   out dx, al                      
3E71  c3                   ret                             
3E72  eb 93                jmp 0x3e07                      
3E74  b0 47                mov al, 0x47                    
3E76  ee                   out dx, al                      
3E77  83 c2 02             add dx, 2                       
3E7A  b0 01                mov al, 1                       
3E7C  ee                   out dx, al                      
3E7D  ee                   out dx, al                      
3E7E  ee                   out dx, al                      
3E7F  ee                   out dx, al                      
3E80  b0 05                mov al, 5                       
3E82  ee                   out dx, al                      
3E83  b0 ff                mov al, 0xff                    
3E85  83 ea 02             sub dx, 2                       
3E88  ee                   out dx, al                      
3E89  ee                   out dx, al                      
3E8A  42                   inc dx                          
3E8B  42                   inc dx                          
3E8C  83 e9 02             sub cx, 2                       
3E8F  bb 04 05             mov bx, 0x504                   
3E92  d1 e9                shr cx, 1                       
3E94  0b c9                or cx, cx                       
3E96  74 31                je 0x3ec9                       
3E98  8a c3                mov al, bl                      
3E9A  ee                   out dx, al                      
3E9B  ee                   out dx, al                      
3E9C  ee                   out dx, al                      
3E9D  4a                   dec dx                          
3E9E  ec                   in al, dx                       
3E9F  8a e0                mov ah, al                      
3EA1  42                   inc dx                          
3EA2  ec                   in al, dx                       
3EA3  ec                   in al, dx                       
3EA4  86 e0                xchg al, ah                     
3EA6  d0 ec                shr ah, 1                       
3EA8  d1 e8                shr ax, 1                       
3EAA  d1 e8                shr ax, 1                       
3EAC  d1 e8                shr ax, 1                       
3EAE  aa                   stosb byte ptr es:[di], al      
3EAF  8a c7                mov al, bh                      
3EB1  ee                   out dx, al                      
3EB2  ee                   out dx, al                      
3EB3  ee                   out dx, al                      
3EB4  4a                   dec dx                          
3EB5  ec                   in al, dx                       
3EB6  8a e0                mov ah, al                      
3EB8  42                   inc dx                          
3EB9  ec                   in al, dx                       
3EBA  ec                   in al, dx                       
3EBB  86 e0                xchg al, ah                     
3EBD  d0 ec                shr ah, 1                       
3EBF  d1 e8                shr ax, 1                       
3EC1  d1 e8                shr ax, 1                       
3EC3  d1 e8                shr ax, 1                       
3EC5  aa                   stosb byte ptr es:[di], al      
3EC6  49                   dec cx                          
3EC7  75 40                jne 0x3f09                      
3EC9  8a c3                mov al, bl                      
3ECB  ee                   out dx, al                      
3ECC  ee                   out dx, al                      
3ECD  ee                   out dx, al                      
3ECE  4a                   dec dx                          
3ECF  ec                   in al, dx                       
3ED0  8a e0                mov ah, al                      
3ED2  42                   inc dx                          
3ED3  ec                   in al, dx                       
3ED4  ec                   in al, dx                       
3ED5  86 e0                xchg al, ah                     
3ED7  d0 ec                shr ah, 1                       
3ED9  d1 e8                shr ax, 1                       
3EDB  d1 e8                shr ax, 1                       
3EDD  d1 e8                shr ax, 1                       
3EDF  aa                   stosb byte ptr es:[di], al      
3EE0  4a                   dec dx                          
3EE1  4a                   dec dx                          
3EE2  b0 fd                mov al, 0xfd                    
3EE4  ee                   out dx, al                      
3EE5  ee                   out dx, al                      
3EE6  42                   inc dx                          
3EE7  42                   inc dx                          
3EE8  8a c7                mov al, bh                      
3EEA  ee                   out dx, al                      
3EEB  ee                   out dx, al                      
3EEC  4a                   dec dx                          
3EED  ec                   in al, dx                       
3EEE  8a e0                mov ah, al                      
3EF0  42                   inc dx                          
3EF1  ec                   in al, dx                       
3EF2  86 e0                xchg al, ah                     
3EF4  4a                   dec dx                          
3EF5  d0 ec                shr ah, 1                       
3EF7  d1 e8                shr ax, 1                       
3EF9  d1 e8                shr ax, 1                       
3EFB  d1 e8                shr ax, 1                       
3EFD  aa                   stosb byte ptr es:[di], al      
3EFE  4a                   dec dx                          
3EFF  b0 00                mov al, 0                       
3F01  ee                   out dx, al                      
3F02  83 c2 02             add dx, 2                       
3F05  b0 04                mov al, 4                       
3F07  ee                   out dx, al                      
3F08  c3                   ret                             
3F09  eb 8d                jmp 0x3e98                      
3F0B  b0 07                mov al, 7                       
3F0D  ee                   out dx, al                      
3F0E  83 c2 02             add dx, 2                       
3F11  b0 01                mov al, 1                       
3F13  ee                   out dx, al                      
3F14  b0 03                mov al, 3                       
3F16  ee                   out dx, al                      
3F17  ee                   out dx, al                      
3F18  ee                   out dx, al                      
3F19  83 ea 02             sub dx, 2                       
3F1C  b0 ff                mov al, 0xff                    
3F1E  ee                   out dx, al                      
3F1F  42                   inc dx                          
3F20  55                   push bp                         
3F21  8b e9                mov bp, cx                      
3F23  83 ed 02             sub bp, 2                       
3F26  d1 ed                shr bp, 1                       
3F28  bb 07 06             mov bx, 0x607                   
3F2B  42                   inc dx                          
3F2C  8a c7                mov al, bh                      
3F2E  ee                   out dx, al                      
3F2F  ee                   out dx, al                      
3F30  4a                   dec dx                          
3F31  b5 f0                mov ch, 0xf0                    
3F33  0b ed                or bp, bp                       
3F35  74 4b                je 0x3f82                       
3F37  ec                   in al, dx                       
3F38  a8 08                test al, 8                      
3F3A  8a c8                mov cl, al                      
3F3C  75 09                jne 0x3f47                      
3F3E  8a c7                mov al, bh                      
3F40  34 02                xor al, 2                       
3F42  42                   inc dx                          
3F43  ee                   out dx, al                      
3F44  ee                   out dx, al                      
3F45  4a                   dec dx                          
3F46  ec                   in al, dx                       
3F47  22 c5                and al, ch                      
3F49  42                   inc dx                          
3F4A  86 c3                xchg bl, al                     
3F4C  ee                   out dx, al                      
3F4D  4a                   dec dx                          
3F4E  86 c3                xchg bl, al                     
3F50  d0 e9                shr cl, 1                       
3F52  d0 e9                shr cl, 1                       
3F54  d0 e9                shr cl, 1                       
3F56  d0 e9                shr cl, 1                       
3F58  0a c1                or al, cl                       
3F5A  aa                   stosb byte ptr es:[di], al      
3F5B  ec                   in al, dx                       
3F5C  a8 08                test al, 8                      
3F5E  8a c8                mov cl, al                      
3F60  75 09                jne 0x3f6b                      
3F62  8a c3                mov al, bl                      
3F64  34 02                xor al, 2                       
3F66  42                   inc dx                          
3F67  ee                   out dx, al                      
3F68  ee                   out dx, al                      
3F69  4a                   dec dx                          
3F6A  ec                   in al, dx                       
3F6B  22 c5                and al, ch                      
3F6D  42                   inc dx                          
3F6E  86 c7                xchg bh, al                     
3F70  ee                   out dx, al                      
3F71  4a                   dec dx                          
3F72  86 c7                xchg bh, al                     
3F74  d0 e9                shr cl, 1                       
3F76  d0 e9                shr cl, 1                       
3F78  d0 e9                shr cl, 1                       
3F7A  d0 e9                shr cl, 1                       
3F7C  0a c1                or al, cl                       
3F7E  aa                   stosb byte ptr es:[di], al      
3F7F  4d                   dec bp                          
3F80  75 52                jne 0x3fd4                      
3F82  ec                   in al, dx                       
3F83  a8 08                test al, 8                      
3F85  8a c8                mov cl, al                      
3F87  75 09                jne 0x3f92                      
3F89  8a c7                mov al, bh                      
3F8B  34 02                xor al, 2                       
3F8D  42                   inc dx                          
3F8E  ee                   out dx, al                      
3F8F  ee                   out dx, al                      
3F90  4a                   dec dx                          
3F91  ec                   in al, dx                       
3F92  22 c5                and al, ch                      
3F94  d0 e9                shr cl, 1                       
3F96  d0 e9                shr cl, 1                       
3F98  d0 e9                shr cl, 1                       
3F9A  d0 e9                shr cl, 1                       
3F9C  0a c1                or al, cl                       
3F9E  aa                   stosb byte ptr es:[di], al      
3F9F  4a                   dec dx                          
3FA0  b0 fd                mov al, 0xfd                    
3FA2  ee                   out dx, al                      
3FA3  ee                   out dx, al                      
3FA4  42                   inc dx                          
3FA5  42                   inc dx                          
3FA6  8a c3                mov al, bl                      
3FA8  ee                   out dx, al                      
3FA9  ee                   out dx, al                      
3FAA  4a                   dec dx                          
3FAB  ec                   in al, dx                       
3FAC  a8 08                test al, 8                      
3FAE  8a c8                mov cl, al                      
3FB0  75 09                jne 0x3fbb                      
3FB2  8a c3                mov al, bl                      
3FB4  34 02                xor al, 2                       
3FB6  42                   inc dx                          
3FB7  ee                   out dx, al                      
3FB8  ee                   out dx, al                      
3FB9  4a                   dec dx                          
3FBA  ec                   in al, dx                       
3FBB  22 c5                and al, ch                      
3FBD  d0 e9                shr cl, 1                       
3FBF  d0 e9                shr cl, 1                       
3FC1  d0 e9                shr cl, 1                       
3FC3  d0 e9                shr cl, 1                       
3FC5  0a c1                or al, cl                       
3FC7  aa                   stosb byte ptr es:[di], al      
3FC8  5d                   pop bp                          
3FC9  4a                   dec dx                          
3FCA  b0 00                mov al, 0                       
3FCC  ee                   out dx, al                      
3FCD  83 c2 02             add dx, 2                       
3FD0  b0 04                mov al, 4                       
3FD2  ee                   out dx, al                      
3FD3  c3                   ret                             
3FD4  e9 60 ff             jmp 0x3f37                      
3FD7  b0 07                mov al, 7                       
3FD9  ee                   out dx, al                      
3FDA  83 c2 02             add dx, 2                       
3FDD  b0 01                mov al, 1                       
3FDF  ee                   out dx, al                      
3FE0  b0 03                mov al, 3                       
3FE2  ee                   out dx, al                      
3FE3  ee                   out dx, al                      
3FE4  ee                   out dx, al                      
3FE5  83 ea 02             sub dx, 2                       
3FE8  b0 ff                mov al, 0xff                    
3FEA  ee                   out dx, al                      
3FEB  42                   inc dx                          
3FEC  55                   push bp                         
3FED  8b e9                mov bp, cx                      
3FEF  83 ed 02             sub bp, 2                       
3FF2  d1 ed                shr bp, 1                       
3FF4  b5 f0                mov ch, 0xf0                    
3FF6  bb 06 07             mov bx, 0x706                   
3FF9  0b ed                or bp, bp                       
3FFB  74 4b                je 0x4048                       
3FFD  b5 f0                mov ch, 0xf0                    
3FFF  42                   inc dx                          
4000  8a c3                mov al, bl                      
4002  ee                   out dx, al                      
4003  ee                   out dx, al                      
4004  4a                   dec dx                          
4005  ec                   in al, dx                       
4006  a8 08                test al, 8                      
4008  8a c8                mov cl, al                      
400A  75 09                jne 0x4015                      
400C  8a c3                mov al, bl                      
400E  34 02                xor al, 2                       
4010  42                   inc dx                          
4011  ee                   out dx, al                      
4012  ee                   out dx, al                      
4013  4a                   dec dx                          
4014  ec                   in al, dx                       
4015  22 c5                and al, ch                      
4017  d0 e9                shr cl, 1                       
4019  d0 e9                shr cl, 1                       
401B  d0 e9                shr cl, 1                       
401D  d0 e9                shr cl, 1                       
401F  0a c1                or al, cl                       
4021  aa                   stosb byte ptr es:[di], al      
4022  42                   inc dx                          
4023  8a c7                mov al, bh                      
4025  ee                   out dx, al                      
4026  ee                   out dx, al                      
4027  4a                   dec dx                          
4028  ec                   in al, dx                       
4029  a8 08                test al, 8                      
402B  8a c8                mov cl, al                      
402D  75 09                jne 0x4038                      
402F  8a c7                mov al, bh                      
4031  34 02                xor al, 2                       
4033  42                   inc dx                          
4034  ee                   out dx, al                      
4035  ee                   out dx, al                      
4036  4a                   dec dx                          
4037  ec                   in al, dx                       
4038  22 c5                and al, ch                      
403A  d0 e9                shr cl, 1                       
403C  d0 e9                shr cl, 1                       
403E  d0 e9                shr cl, 1                       
4040  d0 e9                shr cl, 1                       
4042  0a c1                or al, cl                       
4044  aa                   stosb byte ptr es:[di], al      
4045  4d                   dec bp                          
4046  75 58                jne 0x40a0                      
4048  42                   inc dx                          
4049  8a c3                mov al, bl                      
404B  ee                   out dx, al                      
404C  ee                   out dx, al                      
404D  4a                   dec dx                          
404E  ec                   in al, dx                       
404F  a8 08                test al, 8                      
4051  8a c8                mov cl, al                      
4053  75 09                jne 0x405e                      
4055  8a c3                mov al, bl                      
4057  34 02                xor al, 2                       
4059  42                   inc dx                          
405A  ee                   out dx, al                      
405B  ee                   out dx, al                      
405C  4a                   dec dx                          
405D  ec                   in al, dx                       
405E  22 c5                and al, ch                      
4060  d0 e9                shr cl, 1                       
4062  d0 e9                shr cl, 1                       
4064  d0 e9                shr cl, 1                       
4066  d0 e9                shr cl, 1                       
4068  0a c1                or al, cl                       
406A  aa                   stosb byte ptr es:[di], al      
406B  4a                   dec dx                          
406C  b0 fd                mov al, 0xfd                    
406E  ee                   out dx, al                      
406F  ee                   out dx, al                      
4070  42                   inc dx                          
4071  42                   inc dx                          
4072  8a c7                mov al, bh                      
4074  ee                   out dx, al                      
4075  ee                   out dx, al                      
4076  4a                   dec dx                          
4077  ec                   in al, dx                       
4078  a8 08                test al, 8                      
407A  8a c8                mov cl, al                      
407C  75 09                jne 0x4087                      
407E  8a c7                mov al, bh                      
4080  34 02                xor al, 2                       
4082  42                   inc dx                          
4083  ee                   out dx, al                      
4084  ee                   out dx, al                      
4085  4a                   dec dx                          
4086  ec                   in al, dx                       
4087  22 c5                and al, ch                      
4089  d0 e9                shr cl, 1                       
408B  d0 e9                shr cl, 1                       
408D  d0 e9                shr cl, 1                       
408F  d0 e9                shr cl, 1                       
4091  0a c1                or al, cl                       
4093  aa                   stosb byte ptr es:[di], al      
4094  5d                   pop bp                          
4095  4a                   dec dx                          
4096  b0 00                mov al, 0                       
4098  ee                   out dx, al                      
4099  83 c2 02             add dx, 2                       
409C  b0 04                mov al, 4                       
409E  ee                   out dx, al                      
409F  c3                   ret                             
40A0  e9 5a ff             jmp 0x3ffd                      
40A3  b0 07                mov al, 7                       
40A5  ee                   out dx, al                      
40A6  ee                   out dx, al                      
40A7  83 c2 02             add dx, 2                       
40AA  b0 01                mov al, 1                       
40AC  ee                   out dx, al                      
40AD  ee                   out dx, al                      
40AE  b0 03                mov al, 3                       
40B0  ee                   out dx, al                      
40B1  ee                   out dx, al                      
40B2  ee                   out dx, al                      
40B3  83 ea 02             sub dx, 2                       
40B6  b0 ff                mov al, 0xff                    
40B8  ee                   out dx, al                      
40B9  ee                   out dx, al                      
40BA  42                   inc dx                          
40BB  55                   push bp                         
40BC  8b e9                mov bp, cx                      
40BE  83 ed 02             sub bp, 2                       
40C1  d1 ed                shr bp, 1                       
40C3  bb 06 07             mov bx, 0x706                   
40C6  b5 f0                mov ch, 0xf0                    
40C8  0b ed                or bp, bp                       
40CA  74 4f                je 0x411b                       
40CC  b5 f0                mov ch, 0xf0                    
40CE  42                   inc dx                          
40CF  8a c3                mov al, bl                      
40D1  ee                   out dx, al                      
40D2  ee                   out dx, al                      
40D3  ee                   out dx, al                      
40D4  4a                   dec dx                          
40D5  ec                   in al, dx                       
40D6  a8 08                test al, 8                      
40D8  8a c8                mov cl, al                      
40DA  75 0a                jne 0x40e6                      
40DC  8a c3                mov al, bl                      
40DE  34 02                xor al, 2                       
40E0  42                   inc dx                          
40E1  ee                   out dx, al                      
40E2  ee                   out dx, al                      
40E3  ee                   out dx, al                      
40E4  4a                   dec dx                          
40E5  ec                   in al, dx                       
40E6  22 c5                and al, ch                      
40E8  d0 e9                shr cl, 1                       
40EA  d0 e9                shr cl, 1                       
40EC  d0 e9                shr cl, 1                       
40EE  d0 e9                shr cl, 1                       
40F0  0a c1                or al, cl                       
40F2  aa                   stosb byte ptr es:[di], al      
40F3  42                   inc dx                          
40F4  8a c7                mov al, bh                      
40F6  ee                   out dx, al                      
40F7  ee                   out dx, al                      
40F8  ee                   out dx, al                      
40F9  4a                   dec dx                          
40FA  ec                   in al, dx                       
40FB  a8 08                test al, 8                      
40FD  8a c8                mov cl, al                      
40FF  75 0a                jne 0x410b                      
4101  8a c7                mov al, bh                      
4103  34 02                xor al, 2                       
4105  42                   inc dx                          
4106  ee                   out dx, al                      
4107  ee                   out dx, al                      
4108  ee                   out dx, al                      
4109  4a                   dec dx                          
410A  ec                   in al, dx                       
410B  22 c5                and al, ch                      
410D  d0 e9                shr cl, 1                       
410F  d0 e9                shr cl, 1                       
4111  d0 e9                shr cl, 1                       
4113  d0 e9                shr cl, 1                       
4115  0a c1                or al, cl                       
4117  aa                   stosb byte ptr es:[di], al      
4118  4d                   dec bp                          
4119  75 5e                jne 0x4179                      
411B  42                   inc dx                          
411C  8a c3                mov al, bl                      
411E  ee                   out dx, al                      
411F  ee                   out dx, al                      
4120  ee                   out dx, al                      
4121  4a                   dec dx                          
4122  ec                   in al, dx                       
4123  a8 08                test al, 8                      
4125  8a c8                mov cl, al                      
4127  75 0a                jne 0x4133                      
4129  8a c3                mov al, bl                      
412B  34 02                xor al, 2                       
412D  42                   inc dx                          
412E  ee                   out dx, al                      
412F  ee                   out dx, al                      
4130  ee                   out dx, al                      
4131  4a                   dec dx                          
4132  ec                   in al, dx                       
4133  22 c5                and al, ch                      
4135  d0 e9                shr cl, 1                       
4137  d0 e9                shr cl, 1                       
4139  d0 e9                shr cl, 1                       
413B  d0 e9                shr cl, 1                       
413D  0a c1                or al, cl                       
413F  aa                   stosb byte ptr es:[di], al      
4140  4a                   dec dx                          
4141  b0 fd                mov al, 0xfd                    
4143  ee                   out dx, al                      
4144  ee                   out dx, al                      
4145  ee                   out dx, al                      
4146  42                   inc dx                          
4147  42                   inc dx                          
4148  8a c7                mov al, bh                      
414A  ee                   out dx, al                      
414B  ee                   out dx, al                      
414C  ee                   out dx, al                      
414D  4a                   dec dx                          
414E  ec                   in al, dx                       
414F  a8 08                test al, 8                      
4151  8a c8                mov cl, al                      
4153  75 0a                jne 0x415f                      
4155  8a c7                mov al, bh                      
4157  34 02                xor al, 2                       
4159  42                   inc dx                          
415A  ee                   out dx, al                      
415B  ee                   out dx, al                      
415C  ee                   out dx, al                      
415D  4a                   dec dx                          
415E  ec                   in al, dx                       
415F  22 c5                and al, ch                      
4161  d0 e9                shr cl, 1                       
4163  d0 e9                shr cl, 1                       
4165  d0 e9                shr cl, 1                       
4167  d0 e9                shr cl, 1                       
4169  0a c1                or al, cl                       
416B  aa                   stosb byte ptr es:[di], al      
416C  5d                   pop bp                          
416D  4a                   dec dx                          
416E  b0 00                mov al, 0                       
4170  ee                   out dx, al                      
4171  ee                   out dx, al                      
4172  83 c2 02             add dx, 2                       
4175  b0 04                mov al, 4                       
4177  ee                   out dx, al                      
4178  c3                   ret                             
4179  e9 50 ff             jmp 0x40cc                      
417C  b0 07                mov al, 7                       
417E  ee                   out dx, al                      
417F  ee                   out dx, al                      
4180  ee                   out dx, al                      
4181  83 c2 02             add dx, 2                       
4184  b0 01                mov al, 1                       
4186  ee                   out dx, al                      
4187  ee                   out dx, al                      
4188  ee                   out dx, al                      
4189  b0 03                mov al, 3                       
418B  ee                   out dx, al                      
418C  ee                   out dx, al                      
418D  ee                   out dx, al                      
418E  ee                   out dx, al                      
418F  83 ea 02             sub dx, 2                       
4192  b0 ff                mov al, 0xff                    
4194  ee                   out dx, al                      
4195  ee                   out dx, al                      
4196  ee                   out dx, al                      
4197  42                   inc dx                          
4198  55                   push bp                         
4199  8b e9                mov bp, cx                      
419B  83 ed 02             sub bp, 2                       
419E  d1 ed                shr bp, 1                       
41A0  bb 06 07             mov bx, 0x706                   
41A3  b5 f0                mov ch, 0xf0                    
41A5  0b ed                or bp, bp                       
41A7  74 5d                je 0x4206                       
41A9  b5 f0                mov ch, 0xf0                    
41AB  42                   inc dx                          
41AC  8a c3                mov al, bl                      
41AE  ee                   out dx, al                      
41AF  ee                   out dx, al                      
41B0  ee                   out dx, al                      
41B1  ee                   out dx, al                      
41B2  ee                   out dx, al                      
41B3  ee                   out dx, al                      
41B4  4a                   dec dx                          
41B5  ec                   in al, dx                       
41B6  ec                   in al, dx                       
41B7  a8 08                test al, 8                      
41B9  8a c8                mov cl, al                      
41BB  75 0d                jne 0x41ca                      
41BD  8a c3                mov al, bl                      
41BF  34 02                xor al, 2                       
41C1  42                   inc dx                          
41C2  ee                   out dx, al                      
41C3  ee                   out dx, al                      
41C4  ee                   out dx, al                      
41C5  ee                   out dx, al                      
41C6  ee                   out dx, al                      
41C7  ee                   out dx, al                      
41C8  4a                   dec dx                          
41C9  ec                   in al, dx                       
41CA  22 c5                and al, ch                      
41CC  d0 e9                shr cl, 1                       
41CE  d0 e9                shr cl, 1                       
41D0  d0 e9                shr cl, 1                       
41D2  d0 e9                shr cl, 1                       
41D4  0a c1                or al, cl                       
41D6  aa                   stosb byte ptr es:[di], al      
41D7  42                   inc dx                          
41D8  8a c7                mov al, bh                      
41DA  ee                   out dx, al                      
41DB  ee                   out dx, al                      
41DC  ee                   out dx, al                      
41DD  ee                   out dx, al                      
41DE  ee                   out dx, al                      
41DF  ee                   out dx, al                      
41E0  4a                   dec dx                          
41E1  ec                   in al, dx                       
41E2  ec                   in al, dx                       
41E3  a8 08                test al, 8                      
41E5  8a c8                mov cl, al                      
41E7  75 0d                jne 0x41f6                      
41E9  8a c7                mov al, bh                      
41EB  34 02                xor al, 2                       
41ED  42                   inc dx                          
41EE  ee                   out dx, al                      
41EF  ee                   out dx, al                      
41F0  ee                   out dx, al                      
41F1  ee                   out dx, al                      
41F2  ee                   out dx, al                      
41F3  ee                   out dx, al                      
41F4  4a                   dec dx                          
41F5  ec                   in al, dx                       
41F6  22 c5                and al, ch                      
41F8  d0 e9                shr cl, 1                       
41FA  d0 e9                shr cl, 1                       
41FC  d0 e9                shr cl, 1                       
41FE  d0 e9                shr cl, 1                       
4200  0a c1                or al, cl                       
4202  aa                   stosb byte ptr es:[di], al      
4203  4d                   dec bp                          
4204  75 6a                jne 0x4270                      
4206  42                   inc dx                          
4207  8a c3                mov al, bl                      
4209  ee                   out dx, al                      
420A  ee                   out dx, al                      
420B  ee                   out dx, al                      
420C  ee                   out dx, al                      
420D  ee                   out dx, al                      
420E  ee                   out dx, al                      
420F  4a                   dec dx                          
4210  ec                   in al, dx                       
4211  ec                   in al, dx                       
4212  a8 08                test al, 8                      
4214  8a c8                mov cl, al                      
4216  75 0d                jne 0x4225                      
4218  8a c3                mov al, bl                      
421A  34 02                xor al, 2                       
421C  42                   inc dx                          
421D  ee                   out dx, al                      
421E  ee                   out dx, al                      
421F  ee                   out dx, al                      
4220  ee                   out dx, al                      
4221  ee                   out dx, al                      
4222  ee                   out dx, al                      
4223  4a                   dec dx                          
4224  ec                   in al, dx                       
4225  22 c5                and al, ch                      
4227  d0 e9                shr cl, 1                       
4229  d0 e9                shr cl, 1                       
422B  d0 e9                shr cl, 1                       
422D  d0 e9                shr cl, 1                       
422F  0a c1                or al, cl                       
4231  aa                   stosb byte ptr es:[di], al      
4232  4a                   dec dx                          
4233  b0 fd                mov al, 0xfd                    
4235  ee                   out dx, al                      
4236  ee                   out dx, al                      
4237  ee                   out dx, al                      
4238  ee                   out dx, al                      
4239  42                   inc dx                          
423A  42                   inc dx                          
423B  8a c7                mov al, bh                      
423D  ee                   out dx, al                      
423E  ee                   out dx, al                      
423F  ee                   out dx, al                      
4240  ee                   out dx, al                      
4241  4a                   dec dx                          
4242  ec                   in al, dx                       
4243  a8 08                test al, 8                      
4245  8a c8                mov cl, al                      
4247  75 0a                jne 0x4253                      
4249  8a c7                mov al, bh                      
424B  34 02                xor al, 2                       
424D  42                   inc dx                          
424E  ee                   out dx, al                      
424F  ee                   out dx, al                      
4250  ee                   out dx, al                      
4251  4a                   dec dx                          
4252  ec                   in al, dx                       
4253  22 c5                and al, ch                      
4255  d0 e9                shr cl, 1                       
4257  d0 e9                shr cl, 1                       
4259  d0 e9                shr cl, 1                       
425B  d0 e9                shr cl, 1                       
425D  0a c1                or al, cl                       
425F  aa                   stosb byte ptr es:[di], al      
4260  5d                   pop bp                          
4261  4a                   dec dx                          
4262  b0 00                mov al, 0                       
4264  ee                   out dx, al                      
4265  ee                   out dx, al                      
4266  ee                   out dx, al                      
4267  ee                   out dx, al                      
4268  83 c2 02             add dx, 2                       
426B  b0 04                mov al, 4                       
426D  ee                   out dx, al                      
426E  ee                   out dx, al                      
426F  c3                   ret                             
4270  e9 36 ff             jmp 0x41a9                      
4273  b0 27                mov al, 0x27                    
4275  ee                   out dx, al                      
4276  83 c2 02             add dx, 2                       
4279  b0 01                mov al, 1                       
427B  ee                   out dx, al                      
427C  ee                   out dx, al                      
427D  b0 85                mov al, 0x85                    
427F  ee                   out dx, al                      
4280  ee                   out dx, al                      
4281  ee                   out dx, al                      
4282  4a                   dec dx                          
4283  4a                   dec dx                          
4284  b0 00                mov al, 0                       
4286  ee                   out dx, al                      
4287  ee                   out dx, al                      
4288  83 c2 02             add dx, 2                       
428B  bb 84 85             mov bx, 0x8584                  
428E  42                   inc dx                          
428F  8a c3                mov al, bl                      
4291  ee                   out dx, al                      
4292  ee                   out dx, al                      
4293  4a                   dec dx                          
4294  4a                   dec dx                          
4295  ec                   in al, dx                       
4296  aa                   stosb byte ptr es:[di], al      
4297  42                   inc dx                          
4298  42                   inc dx                          
4299  8a c7                mov al, bh                      
429B  ee                   out dx, al                      
429C  ee                   out dx, al                      
429D  4a                   dec dx                          
429E  4a                   dec dx                          
429F  ec                   in al, dx                       
42A0  aa                   stosb byte ptr es:[di], al      
42A1  42                   inc dx                          
42A2  49                   dec cx                          
42A3  75 29                jne 0x42ce                      
42A5  42                   inc dx                          
42A6  8a c3                mov al, bl                      
42A8  ee                   out dx, al                      
42A9  ee                   out dx, al                      
42AA  4a                   dec dx                          
42AB  4a                   dec dx                          
42AC  ec                   in al, dx                       
42AD  aa                   stosb byte ptr es:[di], al      
42AE  42                   inc dx                          
42AF  8a c3                mov al, bl                      
42B1  0c 02                or al, 2                        
42B3  ee                   out dx, al                      
42B4  ee                   out dx, al                      
42B5  34 01                xor al, 1                       
42B7  ee                   out dx, al                      
42B8  ee                   out dx, al                      
42B9  86 e0                xchg al, ah                     
42BB  83 ea 02             sub dx, 2                       
42BE  ec                   in al, dx                       
42BF  aa                   stosb byte ptr es:[di], al      
42C0  83 c2 02             add dx, 2                       
42C3  86 e0                xchg al, ah                     
42C5  24 fd                and al, 0xfd                    
42C7  ee                   out dx, al                      
42C8  ee                   out dx, al                      
42C9  b0 04                mov al, 4                       
42CB  ee                   out dx, al                      
42CC  ee                   out dx, al                      
42CD  c3                   ret                             
42CE  eb be                jmp 0x428e                      
42D0  b0 27                mov al, 0x27                    
42D2  ee                   out dx, al                      
42D3  83 c2 02             add dx, 2                       
42D6  b0 01                mov al, 1                       
42D8  ee                   out dx, al                      
42D9  ee                   out dx, al                      
42DA  b0 85                mov al, 0x85                    
42DC  ee                   out dx, al                      
42DD  ee                   out dx, al                      
42DE  ee                   out dx, al                      
42DF  4a                   dec dx                          
42E0  4a                   dec dx                          
42E1  b0 00                mov al, 0                       
42E3  ee                   out dx, al                      
42E4  ee                   out dx, al                      
42E5  83 c2 02             add dx, 2                       
42E8  bb 84 85             mov bx, 0x8584                  
42EB  42                   inc dx                          
42EC  8a c3                mov al, bl                      
42EE  ee                   out dx, al                      
42EF  ee                   out dx, al                      
42F0  4a                   dec dx                          
42F1  4a                   dec dx                          
42F2  ec                   in al, dx                       
42F3  aa                   stosb byte ptr es:[di], al      
42F4  42                   inc dx                          
42F5  42                   inc dx                          
42F6  8a c7                mov al, bh                      
42F8  ee                   out dx, al                      
42F9  ee                   out dx, al                      
42FA  4a                   dec dx                          
42FB  4a                   dec dx                          
42FC  ec                   in al, dx                       
42FD  aa                   stosb byte ptr es:[di], al      
42FE  42                   inc dx                          
42FF  49                   dec cx                          
4300  75 29                jne 0x432b                      
4302  42                   inc dx                          
4303  8a c3                mov al, bl                      
4305  ee                   out dx, al                      
4306  ee                   out dx, al                      
4307  4a                   dec dx                          
4308  4a                   dec dx                          
4309  ec                   in al, dx                       
430A  aa                   stosb byte ptr es:[di], al      
430B  42                   inc dx                          
430C  8a c3                mov al, bl                      
430E  0c 02                or al, 2                        
4310  ee                   out dx, al                      
4311  ee                   out dx, al                      
4312  34 01                xor al, 1                       
4314  ee                   out dx, al                      
4315  ee                   out dx, al                      
4316  86 e0                xchg al, ah                     
4318  83 ea 02             sub dx, 2                       
431B  ec                   in al, dx                       
431C  aa                   stosb byte ptr es:[di], al      
431D  83 c2 02             add dx, 2                       
4320  86 e0                xchg al, ah                     
4322  24 fd                and al, 0xfd                    
4324  ee                   out dx, al                      
4325  ee                   out dx, al                      
4326  b0 04                mov al, 4                       
4328  ee                   out dx, al                      
4329  ee                   out dx, al                      
432A  c3                   ret                             
432B  eb be                jmp 0x42eb                      
432D  b0 27                mov al, 0x27                    
432F  ee                   out dx, al                      
4330  83 c2 02             add dx, 2                       
4333  b0 01                mov al, 1                       
4335  ee                   out dx, al                      
4336  ee                   out dx, al                      
4337  b0 25                mov al, 0x25                    
4339  ee                   out dx, al                      
433A  ee                   out dx, al                      
433B  ee                   out dx, al                      
433C  4a                   dec dx                          
433D  4a                   dec dx                          
433E  b0 00                mov al, 0                       
4340  ee                   out dx, al                      
4341  ee                   out dx, al                      
4342  42                   inc dx                          
4343  bb 24 25             mov bx, 0x2524                  
4346  83 e9 02             sub cx, 2                       
4349  d1 e9                shr cx, 1                       
434B  0b c9                or cx, cx                       
434D  74 15                je 0x4364                       
434F  42                   inc dx                          
4350  8a c3                mov al, bl                      
4352  ee                   out dx, al                      
4353  4a                   dec dx                          
4354  4a                   dec dx                          
4355  ec                   in al, dx                       
4356  aa                   stosb byte ptr es:[di], al      
4357  42                   inc dx                          
4358  42                   inc dx                          
4359  8a c7                mov al, bh                      
435B  ee                   out dx, al                      
435C  4a                   dec dx                          
435D  4a                   dec dx                          
435E  ec                   in al, dx                       
435F  aa                   stosb byte ptr es:[di], al      
4360  42                   inc dx                          
4361  49                   dec cx                          
4362  75 2a                jne 0x438e                      
4364  42                   inc dx                          
4365  8a c3                mov al, bl                      
4367  ee                   out dx, al                      
4368  ee                   out dx, al                      
4369  4a                   dec dx                          
436A  4a                   dec dx                          
436B  ec                   in al, dx                       
436C  aa                   stosb byte ptr es:[di], al      
436D  42                   inc dx                          
436E  42                   inc dx                          
436F  8a c3                mov al, bl                      
4371  0c 02                or al, 2                        
4373  ee                   out dx, al                      
4374  ee                   out dx, al                      
4375  34 01                xor al, 1                       
4377  ee                   out dx, al                      
4378  ee                   out dx, al                      
4379  86 e0                xchg al, ah                     
437B  83 ea 02             sub dx, 2                       
437E  ec                   in al, dx                       
437F  aa                   stosb byte ptr es:[di], al      
4380  83 c2 02             add dx, 2                       
4383  86 e0                xchg al, ah                     
4385  24 fd                and al, 0xfd                    
4387  ee                   out dx, al                      
4388  ee                   out dx, al                      
4389  b0 04                mov al, 4                       
438B  ee                   out dx, al                      
438C  ee                   out dx, al                      
438D  c3                   ret                             
438E  eb bf                jmp 0x434f                      
4390  b0 27                mov al, 0x27                    
4392  ee                   out dx, al                      
4393  83 c2 02             add dx, 2                       
4396  b0 01                mov al, 1                       
4398  ee                   out dx, al                      
4399  ee                   out dx, al                      
439A  b0 25                mov al, 0x25                    
439C  ee                   out dx, al                      
439D  ee                   out dx, al                      
439E  ee                   out dx, al                      
439F  4a                   dec dx                          
43A0  4a                   dec dx                          
43A1  b0 00                mov al, 0                       
43A3  ee                   out dx, al                      
43A4  ee                   out dx, al                      
43A5  42                   inc dx                          
43A6  83 e9 02             sub cx, 2                       
43A9  d1 e9                shr cx, 1                       
43AB  bb 24 25             mov bx, 0x2524                  
43AE  0b c9                or cx, cx                       
43B0  74 17                je 0x43c9                       
43B2  42                   inc dx                          
43B3  8a c3                mov al, bl                      
43B5  ee                   out dx, al                      
43B6  ee                   out dx, al                      
43B7  4a                   dec dx                          
43B8  4a                   dec dx                          
43B9  ec                   in al, dx                       
43BA  aa                   stosb byte ptr es:[di], al      
43BB  42                   inc dx                          
43BC  42                   inc dx                          
43BD  8a c7                mov al, bh                      
43BF  ee                   out dx, al                      
43C0  ee                   out dx, al                      
43C1  4a                   dec dx                          
43C2  4a                   dec dx                          
43C3  ec                   in al, dx                       
43C4  aa                   stosb byte ptr es:[di], al      
43C5  42                   inc dx                          
43C6  49                   dec cx                          
43C7  75 2a                jne 0x43f3                      
43C9  42                   inc dx                          
43CA  8a c3                mov al, bl                      
43CC  ee                   out dx, al                      
43CD  ee                   out dx, al                      
43CE  4a                   dec dx                          
43CF  4a                   dec dx                          
43D0  ec                   in al, dx                       
43D1  aa                   stosb byte ptr es:[di], al      
43D2  42                   inc dx                          
43D3  42                   inc dx                          
43D4  8a c3                mov al, bl                      
43D6  0c 02                or al, 2                        
43D8  ee                   out dx, al                      
43D9  ee                   out dx, al                      
43DA  34 01                xor al, 1                       
43DC  ee                   out dx, al                      
43DD  ee                   out dx, al                      
43DE  86 e0                xchg al, ah                     
43E0  83 ea 02             sub dx, 2                       
43E3  ec                   in al, dx                       
43E4  aa                   stosb byte ptr es:[di], al      
43E5  83 c2 02             add dx, 2                       
43E8  86 e0                xchg al, ah                     
43EA  24 fd                and al, 0xfd                    
43EC  ee                   out dx, al                      
43ED  ee                   out dx, al                      
43EE  b0 04                mov al, 4                       
43F0  ee                   out dx, al                      
43F1  ee                   out dx, al                      
43F2  c3                   ret                             
43F3  eb bd                jmp 0x43b2                      
43F5  9c                   pushf                           
43F6  53                   push bx                         
43F7  56                   push si                         
43F8  52                   push dx                         
43F9  fc                   cld                             
43FA  80 3e c7 0b 01       cmp byte ptr [0xbc7], 1         
43FF  75 14                jne 0x4415                      
4401  83 c2 03             add dx, 3                       
4404  ec                   in al, dx                       
4405  24 f8                and al, 0xf8                    
4407  0c 06                or al, 6                        
4409  ee                   out dx, al                      
440A  83 c2 03             add dx, 3                       
440D  b0 80                mov al, 0x80                    
440F  ee                   out dx, al                      
4410  83 ea 02             sub dx, 2                       
4413  eb 43                jmp 0x4458                      
4415  80 3e c0 0b 03       cmp byte ptr [0xbc0], 3         
441A  75 19                jne 0x4435                      
441C  50                   push ax                         
441D  b8 00 80             mov ax, 0x8000                  
4420  e6 23                out 0x23, al                    
4422  eb 00                jmp 0x4424                      
4424  86 e0                xchg al, ah                     
4426  e6 22                out 0x22, al                    
4428  eb 00                jmp 0x442a                      
442A  e7 22                out 0x22, ax                    
442C  e4 22                in al, 0x22                     
442E  24 1f                and al, 0x1f                    
4430  0c 21                or al, 0x21                     
4432  e6 22                out 0x22, al                    
4434  58                   pop ax                          
4435  83 c2 03             add dx, 3                       
4438  b0 80                mov al, 0x80                    
443A  ee                   out dx, al                      
443B  42                   inc dx                          
443C  80 3e c0 0b 03       cmp byte ptr [0xbc0], 3         
4441  75 15                jne 0x4458                      
4443  50                   push ax                         
4444  b8 00 80             mov ax, 0x8000                  
4447  e6 23                out 0x23, al                    
4449  86 e0                xchg al, ah                     
444B  e6 22                out 0x22, al                    
444D  e7 22                out 0x22, ax                    
444F  e4 22                in al, 0x22                     
4451  24 1f                and al, 0x1f                    
4453  0c 01                or al, 1                        
4455  e6 22                out 0x22, al                    
4457  58                   pop ax                          
4458  83 ea 02             sub dx, 2                       
445B  ec                   in al, dx                       
445C  24 1f                and al, 0x1f                    
445E  0c 20                or al, 0x20                     
4460  ee                   out dx, al                      
4461  83 c2 02             add dx, 2                       
4464  49                   dec cx                          
4465  f3 6c                rep insb byte ptr es:[di], dx   
4467  83 ea 02             sub dx, 2                       
446A  ec                   in al, dx                       
446B  24 1f                and al, 0x1f                    
446D  ee                   out dx, al                      
446E  83 c2 02             add dx, 2                       
4471  4a                   dec dx                          
4472  80 3e c7 0b 01       cmp byte ptr [0xbc7], 1         
4477  75 0b                jne 0x4484                      
4479  83 c2 03             add dx, 3                       
447C  b0 a0                mov al, 0xa0                    
447E  ee                   out dx, al                      
447F  83 ea 02             sub dx, 2                       
4482  eb 40                jmp 0x44c4                      
4484  80 3e c0 0b 03       cmp byte ptr [0xbc0], 3         
4489  75 19                jne 0x44a4                      
448B  50                   push ax                         
448C  b8 00 80             mov ax, 0x8000                  
448F  e6 23                out 0x23, al                    
4491  eb 00                jmp 0x4493                      
4493  86 e0                xchg al, ah                     
4495  e6 22                out 0x22, al                    
4497  eb 00                jmp 0x4499                      
4499  e7 22                out 0x22, ax                    
449B  e4 22                in al, 0x22                     
449D  24 1f                and al, 0x1f                    
449F  0c 21                or al, 0x21                     
44A1  e6 22                out 0x22, al                    
44A3  58                   pop ax                          
44A4  b0 a0                mov al, 0xa0                    
44A6  ee                   out dx, al                      
44A7  42                   inc dx                          
44A8  80 3e c0 0b 03       cmp byte ptr [0xbc0], 3         
44AD  75 15                jne 0x44c4                      
44AF  50                   push ax                         
44B0  b8 00 80             mov ax, 0x8000                  
44B3  e6 23                out 0x23, al                    
44B5  86 e0                xchg al, ah                     
44B7  e6 22                out 0x22, al                    
44B9  e7 22                out 0x22, ax                    
44BB  e4 22                in al, 0x22                     
44BD  24 1f                and al, 0x1f                    
44BF  0c 01                or al, 1                        
44C1  e6 22                out 0x22, al                    
44C3  58                   pop ax                          
44C4  83 ea 02             sub dx, 2                       
44C7  ec                   in al, dx                       
44C8  24 1f                and al, 0x1f                    
44CA  0c 20                or al, 0x20                     
44CC  ee                   out dx, al                      
44CD  83 c2 02             add dx, 2                       
44D0  6c                   insb byte ptr es:[di], dx       
44D1  83 ea 02             sub dx, 2                       
44D4  ec                   in al, dx                       
44D5  24 1f                and al, 0x1f                    
44D7  ee                   out dx, al                      
44D8  5a                   pop dx                          
44D9  5e                   pop si                          
44DA  5b                   pop bx                          
44DB  9d                   popf                            
44DC  c3                   ret                             
44DD  9c                   pushf                           
44DE  53                   push bx                         
44DF  56                   push si                         
44E0  52                   push dx                         
44E1  fc                   cld                             
44E2  80 3e c7 0b 01       cmp byte ptr [0xbc7], 1         
44E7  75 18                jne 0x4501                      
44E9  ec                   in al, dx                       
44EA  24 f8                and al, 0xf8                    
44EC  0c 06                or al, 6                        
44EE  ee                   out dx, al                      
44EF  83 c2 03             add dx, 3                       
44F2  b0 80                mov al, 0x80                    
44F4  ee                   out dx, al                      
44F5  83 ea 03             sub dx, 3                       
44F8  ec                   in al, dx                       
44F9  24 f8                and al, 0xf8                    
44FB  0c 07                or al, 7                        
44FD  ee                   out dx, al                      
44FE  42                   inc dx                          
44FF  eb 43                jmp 0x4544                      
4501  80 3e c0 0b 03       cmp byte ptr [0xbc0], 3         
4506  75 19                jne 0x4521                      
4508  50                   push ax                         
4509  b8 00 80             mov ax, 0x8000                  
450C  e6 23                out 0x23, al                    
450E  eb 00                jmp 0x4510                      
4510  86 e0                xchg al, ah                     
4512  e6 22                out 0x22, al                    
4514  eb 00                jmp 0x4516                      
4516  e7 22                out 0x22, ax                    
4518  e4 22                in al, 0x22                     
451A  24 1f                and al, 0x1f                    
451C  0c 21                or al, 0x21                     
451E  e6 22                out 0x22, al                    
4520  58                   pop ax                          
4521  83 c2 03             add dx, 3                       
4524  b0 80                mov al, 0x80                    
4526  ee                   out dx, al                      
4527  42                   inc dx                          
4528  80 3e c0 0b 03       cmp byte ptr [0xbc0], 3         
452D  75 15                jne 0x4544                      
452F  50                   push ax                         
4530  b8 00 80             mov ax, 0x8000                  
4533  e6 23                out 0x23, al                    
4535  86 e0                xchg al, ah                     
4537  e6 22                out 0x22, al                    
4539  e7 22                out 0x22, ax                    
453B  e4 22                in al, 0x22                     
453D  24 1f                and al, 0x1f                    
453F  0c 01                or al, 1                        
4541  e6 22                out 0x22, al                    
4543  58                   pop ax                          
4544  83 ea 02             sub dx, 2                       
4547  ec                   in al, dx                       
4548  24 1f                and al, 0x1f                    
454A  0c 20                or al, 0x20                     
454C  ee                   out dx, al                      
454D  83 c2 02             add dx, 2                       
4550  f7 c1 03 00          test cx, 3                      
4554  74 19                je 0x456f                       
4556  83 f9 04             cmp cx, 4                       
4559  72 0c                jb 0x4567                       
455B  51                   push cx                         
455C  83 e1 03             and cx, 3                       
455F  f3 6c                rep insb byte ptr es:[di], dx   
4561  59                   pop cx                          
4562  83 e1 fc             and cx, 0xfffc                  
4565  eb 08                jmp 0x456f                      
4567  83 f9 01             cmp cx, 1                       
456A  74 1d                je 0x4589                       
456C  49                   dec cx                          
456D  eb 0e                jmp 0x457d                      
456F  83 e9 04             sub cx, 4                       
4572  74 06                je 0x457a                       
4574  c1 e9 02             shr cx, 2                       
4577  f3 66 6d             rep insd dword ptr es:[di], dx  
457A  b9 03 00             mov cx, 3                       
457D  f3 6c                rep insb byte ptr es:[di], dx   
457F  83 ea 02             sub dx, 2                       
4582  ec                   in al, dx                       
4583  24 1f                and al, 0x1f                    
4585  ee                   out dx, al                      
4586  83 c2 02             add dx, 2                       
4589  4a                   dec dx                          
458A  80 3e c7 0b 01       cmp byte ptr [0xbc7], 1         
458F  75 11                jne 0x45a2                      
4591  ec                   in al, dx                       
4592  24 f8                and al, 0xf8                    
4594  0c 06                or al, 6                        
4596  ee                   out dx, al                      
4597  83 c2 03             add dx, 3                       
459A  b0 a0                mov al, 0xa0                    
459C  ee                   out dx, al                      
459D  83 ea 02             sub dx, 2                       
45A0  eb 40                jmp 0x45e2                      
45A2  80 3e c0 0b 03       cmp byte ptr [0xbc0], 3         
45A7  75 19                jne 0x45c2                      
45A9  50                   push ax                         
45AA  b8 00 80             mov ax, 0x8000                  
45AD  e6 23                out 0x23, al                    
45AF  eb 00                jmp 0x45b1                      
45B1  86 e0                xchg al, ah                     
45B3  e6 22                out 0x22, al                    
45B5  eb 00                jmp 0x45b7                      
45B7  e7 22                out 0x22, ax                    
45B9  e4 22                in al, 0x22                     
45BB  24 1f                and al, 0x1f                    
45BD  0c 21                or al, 0x21                     
45BF  e6 22                out 0x22, al                    
45C1  58                   pop ax                          
45C2  b0 a0                mov al, 0xa0                    
45C4  ee                   out dx, al                      
45C5  80 3e c0 0b 03       cmp byte ptr [0xbc0], 3         
45CA  75 15                jne 0x45e1                      
45CC  50                   push ax                         
45CD  b8 00 80             mov ax, 0x8000                  
45D0  e6 23                out 0x23, al                    
45D2  86 e0                xchg al, ah                     
45D4  e6 22                out 0x22, al                    
45D6  e7 22                out 0x22, ax                    
45D8  e4 22                in al, 0x22                     
45DA  24 1f                and al, 0x1f                    
45DC  0c 01                or al, 1                        
45DE  e6 22                out 0x22, al                    
45E0  58                   pop ax                          
45E1  42                   inc dx                          
45E2  83 ea 02             sub dx, 2                       
45E5  ec                   in al, dx                       
45E6  24 1f                and al, 0x1f                    
45E8  0c 20                or al, 0x20                     
45EA  ee                   out dx, al                      
45EB  83 c2 02             add dx, 2                       
45EE  6c                   insb byte ptr es:[di], dx       
45EF  83 ea 02             sub dx, 2                       
45F2  ec                   in al, dx                       
45F3  24 1f                and al, 0x1f                    
45F5  ee                   out dx, al                      
45F6  5a                   pop dx                          
45F7  5e                   pop si                          
45F8  5b                   pop bx                          
45F9  9d                   popf                            
45FA  c3                   ret                             
45FB  9c                   pushf                           
45FC  51                   push cx                         
45FD  fc                   cld                             
45FE  fa                   cli                             
45FF  b0 80                mov al, 0x80                    
4601  55                   push bp                         
4602  b4 0d                mov ah, 0xd                        ; 000D='IMGR'
4604  8a 16 c9 0b          mov dl, byte ptr [0xbc9]        
4608  57                   push di                         
4609  49                   dec cx                          
460A  51                   push cx                         
460B  ff 1e e1 0b          lcall [0xbe1]                   
460F  59                   pop cx                          
4610  5f                   pop di                          
4611  03 f9                add di, cx                      
4613  b0 a0                mov al, 0xa0                    
4615  8a 16 c9 0b          mov dl, byte ptr [0xbc9]        
4619  b4 0b                mov ah, 0xb                        ; 000B='CSIMGR'
461B  57                   push di                         
461C  ff 1e e1 0b          lcall [0xbe1]                   
4620  5f                   pop di                          
4621  aa                   stosb byte ptr es:[di], al      
4622  47                   inc di                          
4623  5d                   pop bp                          
4624  59                   pop cx                          
4625  9d                   popf                            
4626  c3                   ret                             
4627  55                   push bp                         
4628  51                   push cx                         
4629  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
462D  81 c2 02 04          add dx, 0x402                   
4631  b0 14                mov al, 0x14                    
4633  ee                   out dx, al                      
4634  eb 00                jmp 0x4636                      
4636  81 ea 00 04          sub dx, 0x400                   
463A  b0 04                mov al, 4                       
463C  ee                   out dx, al                      
463D  81 c2 00 04          add dx, 0x400                   
4641  c7 06 1d 0c 00 00    mov word ptr [0xc1d], 0         
4647  39 0e 1a 0c          cmp word ptr [0xc1a], cx        
464B  75 03                jne 0x4650                      
464D  e9 c1 00             jmp 0x4711                      
4650  b0 74                mov al, 0x74                    
4652  ee                   out dx, al                      
4653  89 0e 1a 0c          mov word ptr [0xc1a], cx        
4657  51                   push cx                         
4658  b9 00 80             mov cx, 0x8000                  
465B  ec                   in al, dx                       
465C  a8 01                test al, 1                      
465E  e1 fb                loope 0x465b                    
4660  0b c9                or cx, cx                       
4662  59                   pop cx                          
4663  75 03                jne 0x4668                      
4665  e9 b5 01             jmp 0x481d                      
4668  81 ea 02 04          sub dx, 0x402                   
466C  b0 0e                mov al, 0xe                     
466E  ee                   out dx, al                      
466F  81 c2 02 04          add dx, 0x402                   
4673  51                   push cx                         
4674  b9 00 80             mov cx, 0x8000                  
4677  ec                   in al, dx                       
4678  a8 01                test al, 1                      
467A  e1 fb                loope 0x4677                    
467C  0b c9                or cx, cx                       
467E  59                   pop cx                          
467F  75 03                jne 0x4684                      
4681  e9 99 01             jmp 0x481d                      
4684  83 ea 02             sub dx, 2                       
4687  b0 0b                mov al, 0xb                        ; 000B='CSIMGR'
4689  ee                   out dx, al                      
468A  83 c2 02             add dx, 2                       
468D  51                   push cx                         
468E  b9 00 80             mov cx, 0x8000                  
4691  ec                   in al, dx                       
4692  a8 01                test al, 1                      
4694  e1 fb                loope 0x4691                    
4696  0b c9                or cx, cx                       
4698  59                   pop cx                          
4699  75 03                jne 0x469e                      
469B  e9 7f 01             jmp 0x481d                      
469E  81 ea 02 04          sub dx, 0x402                   
46A2  b0 0f                mov al, 0xf                     
46A4  ee                   out dx, al                      
46A5  81 c2 02 04          add dx, 0x402                   
46A9  51                   push cx                         
46AA  b9 00 80             mov cx, 0x8000                  
46AD  ec                   in al, dx                       
46AE  a8 01                test al, 1                      
46B0  e1 fb                loope 0x46ad                    
46B2  0b c9                or cx, cx                       
46B4  59                   pop cx                          
46B5  75 03                jne 0x46ba                      
46B7  e9 63 01             jmp 0x481d                      
46BA  83 ea 02             sub dx, 2                       
46BD  8a c5                mov al, ch                      
46BF  ee                   out dx, al                      
46C0  83 c2 02             add dx, 2                       
46C3  51                   push cx                         
46C4  b9 00 80             mov cx, 0x8000                  
46C7  ec                   in al, dx                       
46C8  a8 01                test al, 1                      
46CA  e1 fb                loope 0x46c7                    
46CC  0b c9                or cx, cx                       
46CE  59                   pop cx                          
46CF  75 03                jne 0x46d4                      
46D1  e9 49 01             jmp 0x481d                      
46D4  80 3e 5c 0c c3       cmp byte ptr [0xc5c], 0xc3      
46D9  76 36                jbe 0x4711                      
46DB  81 ea 02 04          sub dx, 0x402                   
46DF  b0 0b                mov al, 0xb                        ; 000B='CSIMGR'
46E1  ee                   out dx, al                      
46E2  81 c2 02 04          add dx, 0x402                   
46E6  51                   push cx                         
46E7  b9 00 80             mov cx, 0x8000                  
46EA  ec                   in al, dx                       
46EB  a8 01                test al, 1                      
46ED  e1 fb                loope 0x46ea                    
46EF  0b c9                or cx, cx                       
46F1  59                   pop cx                          
46F2  75 03                jne 0x46f7                      
46F4  e9 26 01             jmp 0x481d                      
46F7  83 ea 02             sub dx, 2                       
46FA  8a c1                mov al, cl                      
46FC  ee                   out dx, al                      
46FD  83 c2 02             add dx, 2                       
4700  51                   push cx                         
4701  b9 00 80             mov cx, 0x8000                  
4704  ec                   in al, dx                       
4705  a8 01                test al, 1                      
4707  e1 fb                loope 0x4704                    
4709  0b c9                or cx, cx                       
470B  59                   pop cx                          
470C  75 03                jne 0x4711                      
470E  e9 0c 01             jmp 0x481d                      
4711  b0 34                mov al, 0x34                    
4713  ee                   out dx, al                      
4714  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
4718  83 c2 02             add dx, 2                       
471B  b0 04                mov al, 4                       
471D  ee                   out dx, al                      
471E  b0 74                mov al, 0x74                    
4720  81 c2 00 04          add dx, 0x400                   
4724  ee                   out dx, al                      
4725  51                   push cx                         
4726  b9 00 80             mov cx, 0x8000                  
4729  ec                   in al, dx                       
472A  a8 01                test al, 1                      
472C  e1 fb                loope 0x4729                    
472E  0b c9                or cx, cx                       
4730  59                   pop cx                          
4731  75 03                jne 0x4736                      
4733  e9 e7 00             jmp 0x481d                      
4736  81 ea 02 04          sub dx, 0x402                   
473A  b0 80                mov al, 0x80                    
473C  ee                   out dx, al                      
473D  81 c2 02 04          add dx, 0x402                   
4741  51                   push cx                         
4742  b9 00 80             mov cx, 0x8000                  
4745  ec                   in al, dx                       
4746  a8 01                test al, 1                      
4748  e1 fb                loope 0x4745                    
474A  0b c9                or cx, cx                       
474C  59                   pop cx                          
474D  75 03                jne 0x4752                      
474F  e9 cb 00             jmp 0x481d                      
4752  b0 34                mov al, 0x34                    
4754  ee                   out dx, al                      
4755  81 ea 00 04          sub dx, 0x400                   
4759  b0 20                mov al, 0x20                    
475B  ee                   out dx, al                      
475C  81 c2 00 04          add dx, 0x400                   
4760  b0 74                mov al, 0x74                    
4762  ee                   out dx, al                      
4763  3b 0e 18 0c          cmp cx, word ptr [0xc18]        
4767  bd 01 00             mov bp, 1                       
476A  72 1a                jb 0x4786                       
476C  52                   push dx                         
476D  33 d2                xor dx, dx                      
476F  33 c0                xor ax, ax                      
4771  8b c1                mov ax, cx                      
4773  33 c9                xor cx, cx                      
4775  8b 0e 18 0c          mov cx, word ptr [0xc18]        
4779  f7 f1                div cx                          
477B  8b e8                mov bp, ax                      
477D  8b 0e 18 0c          mov cx, word ptr [0xc18]        
4781  89 16 1d 0c          mov word ptr [0xc1d], dx        
4785  5a                   pop dx                          
4786  52                   push dx                         
4787  8b 16 18 0c          mov dx, word ptr [0xc18]        
478B  4a                   dec dx                          
478C  85 ca                test dx, cx                     
478E  5a                   pop dx                          
478F  75 1d                jne 0x47ae                      
4791  81 ea 01 04          sub dx, 0x401                   
4795  ec                   in al, dx                       
4796  a8 08                test al, 8                      
4798  75 2d                jne 0x47c7                      
479A  81 c2 01 04          add dx, 0x401                   
479E  ec                   in al, dx                       
479F  a8 01                test al, 1                      
47A1  75 09                jne 0x47ac                      
47A3  83 ea 02             sub dx, 2                       
47A6  6c                   insb byte ptr es:[di], dx       
47A7  83 c2 02             add dx, 2                       
47AA  eb f2                jmp 0x479e                      
47AC  eb 6f                jmp 0x481d                      
47AE  51                   push cx                         
47AF  b9 00 80             mov cx, 0x8000                  
47B2  ec                   in al, dx                       
47B3  a8 04                test al, 4                      
47B5  e1 fb                loope 0x47b2                    
47B7  0b c9                or cx, cx                       
47B9  59                   pop cx                          
47BA  74 09                je 0x47c5                       
47BC  83 ea 02             sub dx, 2                       
47BF  6c                   insb byte ptr es:[di], dx       
47C0  83 c2 02             add dx, 2                       
47C3  e2 e9                loop 0x47ae                     
47C5  eb 56                jmp 0x481d                      
47C7  81 c2 01 04          add dx, 0x401                   
47CB  ec                   in al, dx                       
47CC  a8 02                test al, 2                      
47CE  75 2b                jne 0x47fb                      
47D0  3b 0e 18 0c          cmp cx, word ptr [0xc18]        
47D4  72 25                jb 0x47fb                       
47D6  51                   push cx                         
47D7  b9 00 80             mov cx, 0x8000                  
47DA  ec                   in al, dx                       
47DB  a8 02                test al, 2                      
47DD  75 1b                jne 0x47fa                      
47DF  81 ea 01 04          sub dx, 0x401                   
47E3  ec                   in al, dx                       
47E4  a8 08                test al, 8                      
47E6  74 0f                je 0x47f7                       
47E8  81 c2 01 04          add dx, 0x401                   
47EC  e0 ec                loopne 0x47da                   
47EE  0b c9                or cx, cx                       
47F0  59                   pop cx                          
47F1  74 02                je 0x47f5                       
47F3  eb a9                jmp 0x479e                      
47F5  eb 26                jmp 0x481d                      
47F7  59                   pop cx                          
47F8  eb a0                jmp 0x479a                      
47FA  59                   pop cx                          
47FB  83 ea 02             sub dx, 2                       
47FE  f3 6c                rep insb byte ptr es:[di], dx   
4800  4d                   dec bp                          
4801  74 0a                je 0x480d                       
4803  8b 0e 18 0c          mov cx, word ptr [0xc18]        
4807  83 c2 02             add dx, 2                       
480A  e9 79 ff             jmp 0x4786                      
480D  83 3e 1d 0c 00       cmp word ptr [0xc1d], 0         
4812  74 09                je 0x481d                       
4814  8b 0e 1d 0c          mov cx, word ptr [0xc1d]        
4818  83 c2 02             add dx, 2                       
481B  eb 91                jmp 0x47ae                      
481D  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
4821  83 c2 02             add dx, 2                       
4824  b0 04                mov al, 4                       
4826  ee                   out dx, al                      
4827  eb 00                jmp 0x4829                      
4829  81 c2 00 04          add dx, 0x400                   
482D  b0 34                mov al, 0x34                    
482F  ee                   out dx, al                      
4830  59                   pop cx                          
4831  5d                   pop bp                          
4832  c3                   ret                             
4833  57                   push di                         
4834  52                   push dx                         
4835  49                   dec cx                          
4836  54                   push sp                         
4837  45                   inc bp                          
4838  20 46 61             and byte ptr [bp + 0x61], al    
483B  73 74                jae 0x48b1                      
483D  00 00                add byte ptr [bx + si], al      
483F  00 00                add byte ptr [bx + si], al      
4841  00 00                add byte ptr [bx + si], al      
4843  00 00                add byte ptr [bx + si], al      
4845  00 00                add byte ptr [bx + si], al      
4847  0c 60                or al, 0x60                     
4849  ee                   out dx, al                      
484A  83 c2 02             add dx, 2                       
484D  b0 01                mov al, 1                       
484F  ee                   out dx, al                      
4850  ee                   out dx, al                      
4851  83 ea 02             sub dx, 2                       
4854  86 c4                xchg ah, al                     
4856  ee                   out dx, al                      
4857  83 c2 02             add dx, 2                       
485A  b0 04                mov al, 4                       
485C  ee                   out dx, al                      
485D  ee                   out dx, al                      
485E  c3                   ret                             
485F  57                   push di                         
4860  52                   push dx                         
4861  49                   dec cx                          
4862  54                   push sp                         
4863  45                   inc bp                          
4864  20 4e 6f             and byte ptr [bp + 0x6f], cl    
4867  72 6d                jb 0x48d6                       
4869  61                   popaw                           
486A  6c                   insb byte ptr es:[di], dx       
486B  00 00                add byte ptr [bx + si], al      
486D  00 00                add byte ptr [bx + si], al      
486F  00 00                add byte ptr [bx + si], al      
4871  00 00                add byte ptr [bx + si], al      
4873  0c 60                or al, 0x60                     
4875  ee                   out dx, al                      
4876  83 c2 02             add dx, 2                       
4879  b0 01                mov al, 1                       
487B  ee                   out dx, al                      
487C  ee                   out dx, al                      
487D  83 ea 02             sub dx, 2                       
4880  86 c4                xchg ah, al                     
4882  ee                   out dx, al                      
4883  83 c2 02             add dx, 2                       
4886  b0 04                mov al, 4                       
4888  ee                   out dx, al                      
4889  ee                   out dx, al                      
488A  ee                   out dx, al                      
488B  c3                   ret                             
488C  57                   push di                         
488D  52                   push dx                         
488E  49                   dec cx                          
488F  54                   push sp                         
4890  45                   inc bp                          
4891  20 46 61             and byte ptr [bp + 0x61], al    
4894  73 74                jae 0x490a                      
4896  28 2b                sub byte ptr [bp + di], ch      
4898  29 00                sub word ptr [bx + si], ax      
489A  00 00                add byte ptr [bx + si], al      
489C  00 00                add byte ptr [bx + si], al      
489E  00 00                add byte ptr [bx + si], al      
48A0  0c 60                or al, 0x60                     
48A2  ee                   out dx, al                      
48A3  83 c2 02             add dx, 2                       
48A6  b0 01                mov al, 1                       
48A8  ee                   out dx, al                      
48A9  ee                   out dx, al                      
48AA  83 ea 02             sub dx, 2                       
48AD  86 c4                xchg ah, al                     
48AF  ee                   out dx, al                      
48B0  83 c2 02             add dx, 2                       
48B3  b0 04                mov al, 4                       
48B5  ee                   out dx, al                      
48B6  ee                   out dx, al                      
48B7  ee                   out dx, al                      
48B8  c3                   ret                             
48B9  57                   push di                         
48BA  52                   push dx                         
48BB  49                   dec cx                          
48BC  54                   push sp                         
48BD  45                   inc bp                          
48BE  20 53 6c             and byte ptr [bp + di + 0x6c], dl
48C1  6f                   outsw dx, word ptr [si]         
48C2  77 00                ja 0x48c4                       
48C4  00 00                add byte ptr [bx + si], al      
48C6  00 00                add byte ptr [bx + si], al      
48C8  00 00                add byte ptr [bx + si], al      
48CA  00 00                add byte ptr [bx + si], al      
48CC  00 0c                add byte ptr [si], cl           
48CE  60                   pushaw                          
48CF  ee                   out dx, al                      
48D0  ee                   out dx, al                      
48D1  83 c2 02             add dx, 2                       
48D4  b0 01                mov al, 1                       
48D6  ee                   out dx, al                      
48D7  ee                   out dx, al                      
48D8  ee                   out dx, al                      
48D9  83 ea 02             sub dx, 2                       
48DC  86 c4                xchg ah, al                     
48DE  ee                   out dx, al                      
48DF  ee                   out dx, al                      
48E0  83 c2 02             add dx, 2                       
48E3  b0 04                mov al, 4                       
48E5  ee                   out dx, al                      
48E6  ee                   out dx, al                      
48E7  ee                   out dx, al                      
48E8  c3                   ret                             
48E9  57                   push di                         
48EA  52                   push dx                         
48EB  49                   dec cx                          
48EC  54                   push sp                         
48ED  45                   inc bp                          
48EE  20 53 6c             and byte ptr [bp + di + 0x6c], dl
48F1  6f                   outsw dx, word ptr [si]         
48F2  77 28                ja 0x491c                       
48F4  2d 29 00             sub ax, 0x29                    
48F7  00 00                add byte ptr [bx + si], al      
48F9  00 00                add byte ptr [bx + si], al      
48FB  00 00                add byte ptr [bx + si], al      
48FD  0c 60                or al, 0x60                     
48FF  ee                   out dx, al                      
4900  ee                   out dx, al                      
4901  ee                   out dx, al                      
4902  83 c2 02             add dx, 2                       
4905  b0 01                mov al, 1                       
4907  ee                   out dx, al                      
4908  ee                   out dx, al                      
4909  ee                   out dx, al                      
490A  ee                   out dx, al                      
490B  ee                   out dx, al                      
490C  ee                   out dx, al                      
490D  83 ea 02             sub dx, 2                       
4910  86 c4                xchg ah, al                     
4912  ee                   out dx, al                      
4913  ee                   out dx, al                      
4914  ee                   out dx, al                      
4915  83 c2 02             add dx, 2                       
4918  b0 04                mov al, 4                       
491A  ee                   out dx, al                      
491B  ee                   out dx, al                      
491C  ee                   out dx, al                      
491D  c3                   ret                             
491E  45                   inc bp                          
491F  43                   inc bx                          
4920  50                   push ax                         
4921  20 57 72             and byte ptr [bx + 0x72], dl    
4924  69 74 65 00 00       imul si, word ptr [si + 0x65], 0
4929  00 00                add byte ptr [bx + si], al      
492B  00 00                add byte ptr [bx + si], al      
492D  00 00                add byte ptr [bx + si], al      
492F  00 00                add byte ptr [bx + si], al      
4931  00 50 8b             add byte ptr [bx + si - 0x75], dl
4934  16                   push ss                         
4935  fa                   cli                             
4936  0b 81 c2 02          or ax, word ptr [bx + di + 0x2c2]
493A  04 b0                add al, 0xb0                    
493C  14 ee                adc al, 0xee                    
493E  81 ea 00 04          sub dx, 0x400                   
4942  b0 04                mov al, 4                       
4944  ee                   out dx, al                      
4945  81 c2 00 04          add dx, 0x400                   
4949  b0 74                mov al, 0x74                    
494B  ee                   out dx, al                      
494C  51                   push cx                         
494D  b9 ff ff             mov cx, 0xffff                  
4950  ec                   in al, dx                       
4951  a8 01                test al, 1                      
4953  e1 fb                loope 0x4950                    
4955  0b c9                or cx, cx                       
4957  59                   pop cx                          
4958  74 2c                je 0x4986                       
495A  81 ea 02 04          sub dx, 0x402                   
495E  58                   pop ax                          
495F  ee                   out dx, al                      
4960  81 c2 02 04          add dx, 0x402                   
4964  51                   push cx                         
4965  b9 ff ff             mov cx, 0xffff                  
4968  ec                   in al, dx                       
4969  a8 01                test al, 1                      
496B  e1 fb                loope 0x4968                    
496D  0b c9                or cx, cx                       
496F  59                   pop cx                          
4970  74 13                je 0x4985                       
4972  83 ea 02             sub dx, 2                       
4975  8a c4                mov al, ah                      
4977  ee                   out dx, al                      
4978  83 c2 02             add dx, 2                       
497B  51                   push cx                         
497C  b9 ff ff             mov cx, 0xffff                  
497F  ec                   in al, dx                       
4980  a8 01                test al, 1                      
4982  e1 fb                loope 0x497f                    
4984  59                   pop cx                          
4985  50                   push ax                         
4986  b0 34                mov al, 0x34                    
4988  ee                   out dx, al                      
4989  58                   pop ax                          
498A  c3                   ret                             
498B  45                   inc bp                          
498C  50                   push ax                         
498D  50                   push ax                         
498E  20 4e 6f             and byte ptr [bp + 0x6f], cl    
4991  72 6d                jb 0x4a00                       
4993  61                   popaw                           
4994  6c                   insb byte ptr es:[di], dx       
4995  00 00                add byte ptr [bx + si], al      
4997  00 00                add byte ptr [bx + si], al      
4999  00 00                add byte ptr [bx + si], al      
499B  00 00                add byte ptr [bx + si], al      
499D  00 00                add byte ptr [bx + si], al      
499F  0c 40                or al, 0x40                     
49A1  80 3e c7 0b 01       cmp byte ptr [0xbc7], 1         
49A6  75 16                jne 0x49be                      
49A8  83 c2 03             add dx, 3                       
49AB  50                   push ax                         
49AC  ec                   in al, dx                       
49AD  24 f8                and al, 0xf8                    
49AF  0c 06                or al, 6                        
49B1  ee                   out dx, al                      
49B2  58                   pop ax                          
49B3  83 c2 03             add dx, 3                       
49B6  ee                   out dx, al                      
49B7  86 c4                xchg ah, al                     
49B9  83 ea 02             sub dx, 2                       
49BC  eb 27                jmp 0x49e5                      
49BE  80 3e c0 0b 03       cmp byte ptr [0xbc0], 3         
49C3  75 19                jne 0x49de                      
49C5  50                   push ax                         
49C6  b8 00 80             mov ax, 0x8000                  
49C9  e6 23                out 0x23, al                    
49CB  eb 00                jmp 0x49cd                      
49CD  86 e0                xchg al, ah                     
49CF  e6 22                out 0x22, al                    
49D1  eb 00                jmp 0x49d3                      
49D3  e7 22                out 0x22, ax                    
49D5  e4 22                in al, 0x22                     
49D7  24 1f                and al, 0x1f                    
49D9  0c 21                or al, 0x21                     
49DB  e6 22                out 0x22, al                    
49DD  58                   pop ax                          
49DE  83 c2 03             add dx, 3                       
49E1  ee                   out dx, al                      
49E2  86 c4                xchg ah, al                     
49E4  42                   inc dx                          
49E5  ee                   out dx, al                      
49E6  80 3e c0 0b 03       cmp byte ptr [0xbc0], 3         
49EB  75 15                jne 0x4a02                      
49ED  50                   push ax                         
49EE  b8 00 80             mov ax, 0x8000                  
49F1  e6 23                out 0x23, al                    
49F3  86 e0                xchg al, ah                     
49F5  e6 22                out 0x22, al                    
49F7  e7 22                out 0x22, ax                    
49F9  e4 22                in al, 0x22                     
49FB  24 1f                and al, 0x1f                    
49FD  0c 01                or al, 1                        
49FF  e6 22                out 0x22, al                    
4A01  58                   pop ax                          
4A02  c3                   ret                             
4A03  45                   inc bp                          
4A04  50                   push ax                         
4A05  50                   push ax                         
4A06  20 42 49             and byte ptr [bp + si + 0x49], al
4A09  4f                   dec di                          
4A0A  53                   push bx                         
4A0B  28 4e 29             sub byte ptr [bp + 0x29], cl    
4A0E  00 00                add byte ptr [bx + si], al      
4A10  00 00                add byte ptr [bx + si], al      
4A12  00 00                add byte ptr [bx + si], al      
4A14  00 00                add byte ptr [bx + si], al      
4A16  00 0c                add byte ptr [si], cl           
4A18  40                   inc ax                          
4A19  8a f4                mov dh, ah                      
4A1B  b4 0c                mov ah, 0xc                        ; 000C='SIMGR'
4A1D  8a 16 c9 0b          mov dl, byte ptr [0xbc9]        
4A21  ff 1e e1 0b          lcall [0xbe1]                   
4A25  c3                   ret                             
4A26  b0 67                mov al, 0x67                    
4A28  ee                   out dx, al                      
4A29  83 c2 02             add dx, 2                       
4A2C  b0 01                mov al, 1                       
4A2E  ee                   out dx, al                      
4A2F  ee                   out dx, al                      
4A30  b0 05                mov al, 5                       
4A32  ee                   out dx, al                      
4A33  1e                   push ds                         
4A34  06                   push es                         
4A35  1f                   pop ds                          
4A36  b7 04                mov bh, 4                       
4A38  b4 05                mov ah, 5                       
4A3A  4a                   dec dx                          
4A3B  d1 e9                shr cx, 1                       
4A3D  4a                   dec dx                          
4A3E  ac                   lodsb al, byte ptr [si]         
4A3F  ee                   out dx, al                      
4A40  8a c7                mov al, bh                      
4A42  42                   inc dx                          
4A43  42                   inc dx                          
4A44  ee                   out dx, al                      
4A45  4a                   dec dx                          
4A46  4a                   dec dx                          
4A47  ac                   lodsb al, byte ptr [si]         
4A48  ee                   out dx, al                      
4A49  8a c4                mov al, ah                      
4A4B  42                   inc dx                          
4A4C  42                   inc dx                          
4A4D  ee                   out dx, al                      
4A4E  4a                   dec dx                          
4A4F  49                   dec cx                          
4A50  75 0f                jne 0x4a61                      
4A52  1f                   pop ds                          
4A53  42                   inc dx                          
4A54  8a c4                mov al, ah                      
4A56  ee                   out dx, al                      
4A57  ee                   out dx, al                      
4A58  0c 02                or al, 2                        
4A5A  ee                   out dx, al                      
4A5B  ee                   out dx, al                      
4A5C  b0 04                mov al, 4                       
4A5E  ee                   out dx, al                      
4A5F  ee                   out dx, al                      
4A60  c3                   ret                             
4A61  eb da                jmp 0x4a3d                      
4A63  b0 67                mov al, 0x67                    
4A65  ee                   out dx, al                      
4A66  83 c2 02             add dx, 2                       
4A69  b0 01                mov al, 1                       
4A6B  ee                   out dx, al                      
4A6C  ee                   out dx, al                      
4A6D  b0 04                mov al, 4                       
4A6F  ee                   out dx, al                      
4A70  1e                   push ds                         
4A71  06                   push es                         
4A72  1f                   pop ds                          
4A73  bb 0c 04             mov bx, 0x40c                   
4A76  b4 67                mov ah, 0x67                    
4A78  83 ea 02             sub dx, 2                       
4A7B  8a 04                mov al, byte ptr [si]           
4A7D  46                   inc si                          
4A7E  38 e0                cmp al, ah                      
4A80  74 08                je 0x4a8a                       
4A82  ee                   out dx, al                      
4A83  8a e0                mov ah, al                      
4A85  49                   dec cx                          
4A86  75 f3                jne 0x4a7b                      
4A88  eb 0f                jmp 0x4a99                      
4A8A  8a c3                mov al, bl                      
4A8C  83 c2 02             add dx, 2                       
4A8F  ee                   out dx, al                      
4A90  ee                   out dx, al                      
4A91  83 ea 02             sub dx, 2                       
4A94  86 fb                xchg bl, bh                     
4A96  49                   dec cx                          
4A97  75 e2                jne 0x4a7b                      
4A99  1f                   pop ds                          
4A9A  83 c2 02             add dx, 2                       
4A9D  8a c7                mov al, bh                      
4A9F  0c 02                or al, 2                        
4AA1  ee                   out dx, al                      
4AA2  ee                   out dx, al                      
4AA3  b0 04                mov al, 4                       
4AA5  ee                   out dx, al                      
4AA6  c3                   ret                             
4AA7  b0 67                mov al, 0x67                    
4AA9  ee                   out dx, al                      
4AAA  83 c2 02             add dx, 2                       
4AAD  b0 01                mov al, 1                       
4AAF  ee                   out dx, al                      
4AB0  ee                   out dx, al                      
4AB1  b0 05                mov al, 5                       
4AB3  ee                   out dx, al                      
4AB4  1e                   push ds                         
4AB5  06                   push es                         
4AB6  1f                   pop ds                          
4AB7  b7 04                mov bh, 4                       
4AB9  b4 05                mov ah, 5                       
4ABB  4a                   dec dx                          
4ABC  d1 e9                shr cx, 1                       
4ABE  4a                   dec dx                          
4ABF  ac                   lodsb al, byte ptr [si]         
4AC0  ee                   out dx, al                      
4AC1  8a c7                mov al, bh                      
4AC3  42                   inc dx                          
4AC4  42                   inc dx                          
4AC5  ee                   out dx, al                      
4AC6  ee                   out dx, al                      
4AC7  4a                   dec dx                          
4AC8  4a                   dec dx                          
4AC9  ac                   lodsb al, byte ptr [si]         
4ACA  ee                   out dx, al                      
4ACB  8a c4                mov al, ah                      
4ACD  42                   inc dx                          
4ACE  42                   inc dx                          
4ACF  ee                   out dx, al                      
4AD0  ee                   out dx, al                      
4AD1  4a                   dec dx                          
4AD2  49                   dec cx                          
4AD3  75 0f                jne 0x4ae4                      
4AD5  1f                   pop ds                          
4AD6  42                   inc dx                          
4AD7  8a c4                mov al, ah                      
4AD9  ee                   out dx, al                      
4ADA  ee                   out dx, al                      
4ADB  0c 02                or al, 2                        
4ADD  ee                   out dx, al                      
4ADE  ee                   out dx, al                      
4ADF  b0 04                mov al, 4                       
4AE1  ee                   out dx, al                      
4AE2  ee                   out dx, al                      
4AE3  c3                   ret                             
4AE4  eb d8                jmp 0x4abe                      
4AE6  b0 67                mov al, 0x67                    
4AE8  ee                   out dx, al                      
4AE9  83 c2 02             add dx, 2                       
4AEC  b0 01                mov al, 1                       
4AEE  ee                   out dx, al                      
4AEF  ee                   out dx, al                      
4AF0  b0 05                mov al, 5                       
4AF2  ee                   out dx, al                      
4AF3  1e                   push ds                         
4AF4  06                   push es                         
4AF5  1f                   pop ds                          
4AF6  b7 04                mov bh, 4                       
4AF8  b4 05                mov ah, 5                       
4AFA  4a                   dec dx                          
4AFB  d1 e9                shr cx, 1                       
4AFD  4a                   dec dx                          
4AFE  ac                   lodsb al, byte ptr [si]         
4AFF  ee                   out dx, al                      
4B00  ee                   out dx, al                      
4B01  8a c7                mov al, bh                      
4B03  42                   inc dx                          
4B04  42                   inc dx                          
4B05  ee                   out dx, al                      
4B06  ee                   out dx, al                      
4B07  ee                   out dx, al                      
4B08  ee                   out dx, al                      
4B09  4a                   dec dx                          
4B0A  4a                   dec dx                          
4B0B  ac                   lodsb al, byte ptr [si]         
4B0C  ee                   out dx, al                      
4B0D  ee                   out dx, al                      
4B0E  8a c4                mov al, ah                      
4B10  42                   inc dx                          
4B11  42                   inc dx                          
4B12  ee                   out dx, al                      
4B13  ee                   out dx, al                      
4B14  ee                   out dx, al                      
4B15  ee                   out dx, al                      
4B16  4a                   dec dx                          
4B17  49                   dec cx                          
4B18  75 13                jne 0x4b2d                      
4B1A  1f                   pop ds                          
4B1B  42                   inc dx                          
4B1C  8a c4                mov al, ah                      
4B1E  ee                   out dx, al                      
4B1F  ee                   out dx, al                      
4B20  ee                   out dx, al                      
4B21  ee                   out dx, al                      
4B22  0c 02                or al, 2                        
4B24  ee                   out dx, al                      
4B25  ee                   out dx, al                      
4B26  ee                   out dx, al                      
4B27  ee                   out dx, al                      
4B28  b0 04                mov al, 4                       
4B2A  ee                   out dx, al                      
4B2B  ee                   out dx, al                      
4B2C  c3                   ret                             
4B2D  eb ce                jmp 0x4afd                      
4B2F  b0 67                mov al, 0x67                    
4B31  ee                   out dx, al                      
4B32  ee                   out dx, al                      
4B33  ee                   out dx, al                      
4B34  83 c2 02             add dx, 2                       
4B37  b0 01                mov al, 1                       
4B39  ee                   out dx, al                      
4B3A  ee                   out dx, al                      
4B3B  ee                   out dx, al                      
4B3C  ee                   out dx, al                      
4B3D  ee                   out dx, al                      
4B3E  ee                   out dx, al                      
4B3F  b0 05                mov al, 5                       
4B41  ee                   out dx, al                      
4B42  ee                   out dx, al                      
4B43  ee                   out dx, al                      
4B44  ee                   out dx, al                      
4B45  1e                   push ds                         
4B46  06                   push es                         
4B47  1f                   pop ds                          
4B48  b7 04                mov bh, 4                       
4B4A  b4 05                mov ah, 5                       
4B4C  4a                   dec dx                          
4B4D  d1 e9                shr cx, 1                       
4B4F  4a                   dec dx                          
4B50  ac                   lodsb al, byte ptr [si]         
4B51  ee                   out dx, al                      
4B52  ee                   out dx, al                      
4B53  ee                   out dx, al                      
4B54  8a c7                mov al, bh                      
4B56  42                   inc dx                          
4B57  42                   inc dx                          
4B58  ee                   out dx, al                      
4B59  ee                   out dx, al                      
4B5A  ee                   out dx, al                      
4B5B  ee                   out dx, al                      
4B5C  ee                   out dx, al                      
4B5D  ee                   out dx, al                      
4B5E  4a                   dec dx                          
4B5F  4a                   dec dx                          
4B60  ac                   lodsb al, byte ptr [si]         
4B61  ee                   out dx, al                      
4B62  ee                   out dx, al                      
4B63  ee                   out dx, al                      
4B64  8a c4                mov al, ah                      
4B66  42                   inc dx                          
4B67  42                   inc dx                          
4B68  ee                   out dx, al                      
4B69  ee                   out dx, al                      
4B6A  ee                   out dx, al                      
4B6B  ee                   out dx, al                      
4B6C  ee                   out dx, al                      
4B6D  ee                   out dx, al                      
4B6E  4a                   dec dx                          
4B6F  49                   dec cx                          
4B70  75 13                jne 0x4b85                      
4B72  1f                   pop ds                          
4B73  42                   inc dx                          
4B74  8a c4                mov al, ah                      
4B76  ee                   out dx, al                      
4B77  ee                   out dx, al                      
4B78  ee                   out dx, al                      
4B79  ee                   out dx, al                      
4B7A  0c 02                or al, 2                        
4B7C  ee                   out dx, al                      
4B7D  ee                   out dx, al                      
4B7E  ee                   out dx, al                      
4B7F  ee                   out dx, al                      
4B80  b0 04                mov al, 4                       
4B82  ee                   out dx, al                      
4B83  ee                   out dx, al                      
4B84  c3                   ret                             
4B85  eb c8                jmp 0x4b4f                      
4B87  1e                   push ds                         
4B88  53                   push bx                         
4B89  57                   push di                         
4B8A  9c                   pushf                           
4B8B  fc                   cld                             
4B8C  80 3e c7 0b 01       cmp byte ptr [0xbc7], 1         
4B91  75 14                jne 0x4ba7                      
4B93  83 c2 03             add dx, 3                       
4B96  ec                   in al, dx                       
4B97  24 f8                and al, 0xf8                    
4B99  0c 06                or al, 6                        
4B9B  ee                   out dx, al                      
4B9C  83 c2 03             add dx, 3                       
4B9F  b0 c0                mov al, 0xc0                    
4BA1  ee                   out dx, al                      
4BA2  83 ea 02             sub dx, 2                       
4BA5  eb 27                jmp 0x4bce                      
4BA7  80 3e c0 0b 03       cmp byte ptr [0xbc0], 3         
4BAC  75 19                jne 0x4bc7                      
4BAE  50                   push ax                         
4BAF  b8 00 80             mov ax, 0x8000                  
4BB2  e6 23                out 0x23, al                    
4BB4  eb 00                jmp 0x4bb6                      
4BB6  86 e0                xchg al, ah                     
4BB8  e6 22                out 0x22, al                    
4BBA  eb 00                jmp 0x4bbc                      
4BBC  e7 22                out 0x22, ax                    
4BBE  e4 22                in al, 0x22                     
4BC0  24 1f                and al, 0x1f                    
4BC2  0c 21                or al, 0x21                     
4BC4  e6 22                out 0x22, al                    
4BC6  58                   pop ax                          
4BC7  83 c2 03             add dx, 3                       
4BCA  b0 c0                mov al, 0xc0                    
4BCC  ee                   out dx, al                      
4BCD  42                   inc dx                          
4BCE  a0 c0 0b             mov al, byte ptr [0xbc0]        
4BD1  06                   push es                         
4BD2  1f                   pop ds                          
4BD3  f3 6e                rep outsb dx, byte ptr [si]     
4BD5  3c 03                cmp al, 3                       
4BD7  75 15                jne 0x4bee                      
4BD9  50                   push ax                         
4BDA  b8 00 80             mov ax, 0x8000                  
4BDD  e6 23                out 0x23, al                    
4BDF  86 e0                xchg al, ah                     
4BE1  e6 22                out 0x22, al                    
4BE3  e7 22                out 0x22, ax                    
4BE5  e4 22                in al, 0x22                     
4BE7  24 1f                and al, 0x1f                    
4BE9  0c 01                or al, 1                        
4BEB  e6 22                out 0x22, al                    
4BED  58                   pop ax                          
4BEE  9d                   popf                            
4BEF  5f                   pop di                          
4BF0  5b                   pop bx                          
4BF1  1f                   pop ds                          
4BF2  c3                   ret                             
4BF3  0b c9                or cx, cx                       
4BF5  75 01                jne 0x4bf8                      
4BF7  c3                   ret                             
4BF8  1e                   push ds                         
4BF9  53                   push bx                         
4BFA  57                   push di                         
4BFB  9c                   pushf                           
4BFC  fc                   cld                             
4BFD  80 3e c7 0b 01       cmp byte ptr [0xbc7], 1         
4C02  75 1b                jne 0x4c1f                      
4C04  83 c2 03             add dx, 3                       
4C07  ec                   in al, dx                       
4C08  24 f8                and al, 0xf8                    
4C0A  0c 06                or al, 6                        
4C0C  ee                   out dx, al                      
4C0D  83 c2 03             add dx, 3                       
4C10  b0 c0                mov al, 0xc0                    
4C12  ee                   out dx, al                      
4C13  83 ea 03             sub dx, 3                       
4C16  ec                   in al, dx                       
4C17  24 f8                and al, 0xf8                    
4C19  0c 07                or al, 7                        
4C1B  ee                   out dx, al                      
4C1C  42                   inc dx                          
4C1D  eb 27                jmp 0x4c46                      
4C1F  80 3e c0 0b 03       cmp byte ptr [0xbc0], 3         
4C24  75 19                jne 0x4c3f                      
4C26  50                   push ax                         
4C27  b8 00 80             mov ax, 0x8000                  
4C2A  e6 23                out 0x23, al                    
4C2C  eb 00                jmp 0x4c2e                      
4C2E  86 e0                xchg al, ah                     
4C30  e6 22                out 0x22, al                    
4C32  eb 00                jmp 0x4c34                      
4C34  e7 22                out 0x22, ax                    
4C36  e4 22                in al, 0x22                     
4C38  24 1f                and al, 0x1f                    
4C3A  0c 21                or al, 0x21                     
4C3C  e6 22                out 0x22, al                    
4C3E  58                   pop ax                          
4C3F  83 c2 03             add dx, 3                       
4C42  b0 c0                mov al, 0xc0                    
4C44  ee                   out dx, al                      
4C45  42                   inc dx                          
4C46  a0 c0 0b             mov al, byte ptr [0xbc0]        
4C49  06                   push es                         
4C4A  1f                   pop ds                          
4C4B  f7 c1 03 00          test cx, 3                      
4C4F  74 0e                je 0x4c5f                       
4C51  51                   push cx                         
4C52  83 e1 03             and cx, 3                       
4C55  f3 6e                rep outsb dx, byte ptr [si]     
4C57  59                   pop cx                          
4C58  83 e1 fc             and cx, 0xfffc                  
4C5B  0b c9                or cx, cx                       
4C5D  74 06                je 0x4c65                       
4C5F  c1 e9 02             shr cx, 2                       
4C62  f3 66 6f             rep outsd dx, dword ptr [si]    
4C65  3c 03                cmp al, 3                       
4C67  75 15                jne 0x4c7e                      
4C69  50                   push ax                         
4C6A  b8 00 80             mov ax, 0x8000                  
4C6D  e6 23                out 0x23, al                    
4C6F  86 e0                xchg al, ah                     
4C71  e6 22                out 0x22, al                    
4C73  e7 22                out 0x22, ax                    
4C75  e4 22                in al, 0x22                     
4C77  24 1f                and al, 0x1f                    
4C79  0c 01                or al, 1                        
4C7B  e6 22                out 0x22, al                    
4C7D  58                   pop ax                          
4C7E  9d                   popf                            
4C7F  5f                   pop di                          
4C80  5b                   pop bx                          
4C81  1f                   pop ds                          
4C82  c3                   ret                             
4C83  1e                   push ds                         
4C84  06                   push es                         
4C85  9c                   pushf                           
4C86  57                   push di                         
4C87  8a 16 c9 0b          mov dl, byte ptr [0xbc9]        
4C8B  bf e1 0b             mov di, 0xbe1                   
4C8E  fc                   cld                             
4C8F  fa                   cli                             
4C90  b0 c0                mov al, 0xc0                    
4C92  b4 0e                mov ah, 0xe                     
4C94  55                   push bp                         
4C95  56                   push si                         
4C96  51                   push cx                         
4C97  ff 1d                lcall [di]                      
4C99  59                   pop cx                          
4C9A  5e                   pop si                          
4C9B  5d                   pop bp                          
4C9C  03 f1                add si, cx                      
4C9E  5f                   pop di                          
4C9F  9d                   popf                            
4CA0  07                   pop es                          
4CA1  1f                   pop ds                          
4CA2  c3                   ret                             
4CA3  55                   push bp                         
4CA4  51                   push cx                         
4CA5  c7 06 1d 0c 00 00    mov word ptr [0xc1d], 0         
4CAB  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
4CAF  81 c2 02 04          add dx, 0x402                   
4CB3  b0 14                mov al, 0x14                    
4CB5  ee                   out dx, al                      
4CB6  eb 00                jmp 0x4cb8                      
4CB8  81 ea 00 04          sub dx, 0x400                   
4CBC  b0 04                mov al, 4                       
4CBE  ee                   out dx, al                      
4CBF  81 c2 00 04          add dx, 0x400                   
4CC3  39 0e 1a 0c          cmp word ptr [0xc1a], cx        
4CC7  75 03                jne 0x4ccc                      
4CC9  e9 c1 00             jmp 0x4d8d                      
4CCC  b0 74                mov al, 0x74                    
4CCE  ee                   out dx, al                      
4CCF  89 0e 1a 0c          mov word ptr [0xc1a], cx        
4CD3  51                   push cx                         
4CD4  b9 00 80             mov cx, 0x8000                  
4CD7  ec                   in al, dx                       
4CD8  a8 01                test al, 1                      
4CDA  e1 fb                loope 0x4cd7                    
4CDC  0b c9                or cx, cx                       
4CDE  59                   pop cx                          
4CDF  75 03                jne 0x4ce4                      
4CE1  e9 42 01             jmp 0x4e26                      
4CE4  81 ea 02 04          sub dx, 0x402                   
4CE8  b0 0e                mov al, 0xe                     
4CEA  ee                   out dx, al                      
4CEB  81 c2 02 04          add dx, 0x402                   
4CEF  51                   push cx                         
4CF0  b9 00 80             mov cx, 0x8000                  
4CF3  ec                   in al, dx                       
4CF4  a8 01                test al, 1                      
4CF6  e1 fb                loope 0x4cf3                    
4CF8  0b c9                or cx, cx                       
4CFA  59                   pop cx                          
4CFB  75 03                jne 0x4d00                      
4CFD  e9 26 01             jmp 0x4e26                      
4D00  83 ea 02             sub dx, 2                       
4D03  b0 0b                mov al, 0xb                        ; 000B='CSIMGR'
4D05  ee                   out dx, al                      
4D06  83 c2 02             add dx, 2                       
4D09  51                   push cx                         
4D0A  b9 00 80             mov cx, 0x8000                  
4D0D  ec                   in al, dx                       
4D0E  a8 01                test al, 1                      
4D10  e1 fb                loope 0x4d0d                    
4D12  0b c9                or cx, cx                       
4D14  59                   pop cx                          
4D15  75 03                jne 0x4d1a                      
4D17  e9 0c 01             jmp 0x4e26                      
4D1A  81 ea 02 04          sub dx, 0x402                   
4D1E  b0 0f                mov al, 0xf                     
4D20  ee                   out dx, al                      
4D21  81 c2 02 04          add dx, 0x402                   
4D25  51                   push cx                         
4D26  b9 00 80             mov cx, 0x8000                  
4D29  ec                   in al, dx                       
4D2A  a8 01                test al, 1                      
4D2C  e1 fb                loope 0x4d29                    
4D2E  0b c9                or cx, cx                       
4D30  59                   pop cx                          
4D31  75 03                jne 0x4d36                      
4D33  e9 f0 00             jmp 0x4e26                      
4D36  83 ea 02             sub dx, 2                       
4D39  8a c5                mov al, ch                      
4D3B  ee                   out dx, al                      
4D3C  83 c2 02             add dx, 2                       
4D3F  51                   push cx                         
4D40  b9 00 80             mov cx, 0x8000                  
4D43  ec                   in al, dx                       
4D44  a8 01                test al, 1                      
4D46  e1 fb                loope 0x4d43                    
4D48  0b c9                or cx, cx                       
4D4A  59                   pop cx                          
4D4B  75 03                jne 0x4d50                      
4D4D  e9 d6 00             jmp 0x4e26                      
4D50  80 3e 5c 0c c3       cmp byte ptr [0xc5c], 0xc3      
4D55  72 36                jb 0x4d8d                       
4D57  81 ea 02 04          sub dx, 0x402                   
4D5B  b0 0b                mov al, 0xb                        ; 000B='CSIMGR'
4D5D  ee                   out dx, al                      
4D5E  81 c2 02 04          add dx, 0x402                   
4D62  51                   push cx                         
4D63  b9 00 80             mov cx, 0x8000                  
4D66  ec                   in al, dx                       
4D67  a8 01                test al, 1                      
4D69  e1 fb                loope 0x4d66                    
4D6B  0b c9                or cx, cx                       
4D6D  59                   pop cx                          
4D6E  75 03                jne 0x4d73                      
4D70  e9 b3 00             jmp 0x4e26                      
4D73  83 ea 02             sub dx, 2                       
4D76  8a c1                mov al, cl                      
4D78  ee                   out dx, al                      
4D79  83 c2 02             add dx, 2                       
4D7C  51                   push cx                         
4D7D  b9 00 80             mov cx, 0x8000                  
4D80  ec                   in al, dx                       
4D81  a8 01                test al, 1                      
4D83  e1 fb                loope 0x4d80                    
4D85  0b c9                or cx, cx                       
4D87  59                   pop cx                          
4D88  75 03                jne 0x4d8d                      
4D8A  e9 99 00             jmp 0x4e26                      
4D8D  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
4D91  83 c2 02             add dx, 2                       
4D94  b0 04                mov al, 4                       
4D96  ee                   out dx, al                      
4D97  81 c2 00 04          add dx, 0x400                   
4D9B  b0 74                mov al, 0x74                    
4D9D  ee                   out dx, al                      
4D9E  51                   push cx                         
4D9F  b9 00 80             mov cx, 0x8000                  
4DA2  81 ea 01 04          sub dx, 0x401                   
4DA6  ec                   in al, dx                       
4DA7  81 c2 01 04          add dx, 0x401                   
4DAB  a8 08                test al, 8                      
4DAD  74 07                je 0x4db6                       
4DAF  ec                   in al, dx                       
4DB0  a8 01                test al, 1                      
4DB2  e1 ee                loope 0x4da2                    
4DB4  0b c9                or cx, cx                       
4DB6  59                   pop cx                          
4DB7  74 6d                je 0x4e26                       
4DB9  81 ea 02 04          sub dx, 0x402                   
4DBD  b0 c0                mov al, 0xc0                    
4DBF  ee                   out dx, al                      
4DC0  81 c2 02 04          add dx, 0x402                   
4DC4  3b 0e 18 0c          cmp cx, word ptr [0xc18]        
4DC8  bd 01 00             mov bp, 1                       
4DCB  72 20                jb 0x4ded                       
4DCD  52                   push dx                         
4DCE  33 d2                xor dx, dx                      
4DD0  33 c0                xor ax, ax                      
4DD2  8b c1                mov ax, cx                      
4DD4  33 c9                xor cx, cx                      
4DD6  8b 0e 18 0c          mov cx, word ptr [0xc18]        
4DDA  f7 f1                div cx                          
4DDC  8b e8                mov bp, ax                      
4DDE  89 16 1d 0c          mov word ptr [0xc1d], dx        
4DE2  83 e5 ff             and bp, 0xffff                  
4DE5  8b 0e 18 0c          mov cx, word ptr [0xc18]        
4DE9  83 e1 ff             and cx, 0xffff                  
4DEC  5a                   pop dx                          
4DED  ec                   in al, dx                       
4DEE  a8 01                test al, 1                      
4DF0  75 1a                jne 0x4e0c                      
4DF2  51                   push cx                         
4DF3  b9 00 80             mov cx, 0x8000                  
4DF6  ec                   in al, dx                       
4DF7  a8 01                test al, 1                      
4DF9  75 10                jne 0x4e0b                      
4DFB  81 ea 01 04          sub dx, 0x401                   
4DFF  ec                   in al, dx                       
4E00  81 c2 01 04          add dx, 0x401                   
4E04  a8 08                test al, 8                      
4E06  e0 ee                loopne 0x4df6                   
4E08  59                   pop cx                          
4E09  eb 1b                jmp 0x4e26                      
4E0B  59                   pop cx                          
4E0C  83 ea 02             sub dx, 2                       
4E0F  1e                   push ds                         
4E10  06                   push es                         
4E11  1f                   pop ds                          
4E12  f3 6e                rep outsb dx, byte ptr [si]     
4E14  1f                   pop ds                          
4E15  83 c2 02             add dx, 2                       
4E18  4d                   dec bp                          
4E19  74 06                je 0x4e21                       
4E1B  8b 0e 18 0c          mov cx, word ptr [0xc18]        
4E1F  eb cc                jmp 0x4ded                      
4E21  ec                   in al, dx                       
4E22  a8 01                test al, 1                      
4E24  74 fb                je 0x4e21                       
4E26  83 3e 1d 0c 00       cmp word ptr [0xc1d], 0         
4E2B  74 11                je 0x4e3e                       
4E2D  33 c9                xor cx, cx                      
4E2F  8b 0e 1d 0c          mov cx, word ptr [0xc1d]        
4E33  bd 01 00             mov bp, 1                       
4E36  c7 06 1d 0c 00 00    mov word ptr [0xc1d], 0         
4E3C  eb ce                jmp 0x4e0c                      
4E3E  8b 16 fa 0b          mov dx, word ptr [0xbfa]        
4E42  81 c2 02 04          add dx, 0x402                   
4E46  b0 34                mov al, 0x34                    
4E48  ee                   out dx, al                      
4E49  59                   pop cx                          
4E4A  5d                   pop bp                          
4E4B  c3                   ret                             
4E4C  00 00                add byte ptr [bx + si], al      
4E4E  01 01                add word ptr [bx + di], ax      
4E50  02 02                add al, byte ptr [bp + si]      
