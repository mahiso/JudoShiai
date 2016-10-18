/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2006-2016 by Hannu Jokinen
 * Full copyright text is included in the software package.
 */ 

#include <stdio.h>
#include <gtk/gtk.h>

#include "judoshiai.h"

#define NUM_WOMEN 859
#define NUM_MEN   764

unsigned int women_names[NUM_WOMEN] = {
    /* Aada */ 0x0005034b, /* Aamu */ 0x000d6c7f, /* Ada */ 0x00083a1d, /* Adele */ 0x00034c4c, 
    /* Agata */ 0x0009173e, /* Agda */ 0x00087ff9, /* Agnes */ 0x0005025b, /* Agneta */ 0x000c8abf, 
    /* ?got */ 0x000fffff, /* Aija */ 0x00057f7d, /* Aila */ 0x000fd8fb, /* Aili */ 0x000450c9, 
    /* Aina */ 0x0009ba79, /* Aini */ 0x0002324b, /* Ainikki */ 0x0005f9ac, /* Aino */ 0x0001977e, 
    /* Aira */ 0x000ee724, /* Airi */ 0x00056f16, /* Albertina */ 0x000a75a4, /* Aleksandra */ 0x000da96c, 
    /* Aleksiina */ 0x00048ca8, /* Alexandra */ 0x000ecec8, /* Aleyna */ 0x000c9c60, /* Alfa */ 0x000bf29a, 
    /* Alfhild */ 0x00007aaf, /* Alice */ 0x00058108, /* Aliina */ 0x000c80a8, /* Aliisa */ 0x0000ecb4, 
    /* Alina */ 0x00063b5c, /* Alisa */ 0x000a5740, /* Alli */ 0x000f9222, /* Alma */ 0x000f2b51, 
    /* Alva */ 0x0009e0cb, /* Amalia */ 0x0002d5db, /* Amanda */ 0x00087ff8, /* Amelia */ 0x0000428c, 
    /* Amilda */ 0x00088379, /* Anastasia */ 0x000f6bae, /* Andrea */ 0x00019e4f, /* Anelma */ 0x000cfd58, 
    /* Anette */ 0x00046291, /* Anissa */ 0x00019972, /* Anita */ 0x0002090c, /* Anitta */ 0x000f1930, 
    /* Anja */ 0x000a69f8, /* Ann */ 0x0008cf06, /* Anna */ 0x0006acfc, /* Anne */ 0x000b68e5, 
    /* Annele */ 0x000ab48e, /* Anneli */ 0x000cf8a5, /* Annette */ 0x0000d054, /* Anni */ 0x000d24ce, 
    /* Anniina */ 0x0008610d, /* Annika */ 0x000c1f34, /* Annikki */ 0x0000c914, /* Annilotta */ 0x0006eb53, 
    /* Annina */ 0x000beb71, /* Annukka */ 0x0004a901, /* Anri */ 0x000a7993, /* Ansa */ 0x000ac0e0, 
    /* Antonia */ 0x000d31b0, /* Anu */ 0x000d06ea, /* Ariela */ 0x000032ad, /* Arja */ 0x000633ec, 
    /* Arla */ 0x000c946a, /* Armi */ 0x000c2d19, /* Arna */ 0x000af6e8, /* ?sa */ 0x000fffff, 
    /* Asl?g */ 0x000ac236, /* Asta */ 0x00056604, /* Astrid */ 0x00072160, /* Augusta */ 0x000ac57e, 
    /* Auli */ 0x00080add, /* Aulikki */ 0x0001682c, /* Aune */ 0x00082474, /* Auni */ 0x000e685f, 
    /* Aura */ 0x0002bd30, /* Auri */ 0x00093502, /* Auroora */ 0x000897e5, /* Aurora */ 0x000e842a, 
    /* Barbara */ 0x00033822, /* Barbro */ 0x000a7992, /* Beata */ 0x0000a565, /* Beatrice */ 0x000cd7a5, 
    /* Belinda */ 0x0007263c, /* Bella */ 0x0003ae6f, /* Bengta */ 0x000c7f1f, /* Benita */ 0x00025215, 
    /* Berit */ 0x000130bb, /* Berta */ 0x0000b84c, /* Birgit */ 0x0002f2ce, /* Birgitta */ 0x00068084, 
    /* Bodil */ 0x000b974b, /* Briitta */ 0x0007af98, /* Brita */ 0x000d9bfb, /* Britt */ 0x00007f10, 
    /* Britta */ 0x000b111d, /* Camilla */ 0x000c1848, /* Carina */ 0x000c358c, /* Carita */ 0x0001cf57, 
    /* Carola */ 0x00072bbc, /* Carolina */ 0x000ebf76, /* Cecilia */ 0x000a107a, /* Celeste */ 0x0007745b, 
    /* Celina */ 0x000ab3e0, /* Charlotta */ 0x000648e3, /* Christa */ 0x000235c7, /* Christin */ 0x000cf3b9, 
    /* Christina */ 0x000a26dd, /* Christine */ 0x0007e2c4, /* Clara */ 0x000264d9, /* Crista */ 0x0008d43d, 
    /* Daga */ 0x0006a0ba, /* Dagmar */ 0x00015754, /* Dagny */ 0x000ed9ad, /* Daniela */ 0x000a6799, 
    /* Diana */ 0x000b15a6, /* Disa */ 0x000b2657, /* Dora */ 0x000d6ba4, /* Doris */ 0x0008b0dc, 
    /* Dorotea */ 0x000d51b8, /* Dorrit */ 0x000685e8, /* Ea */ 0x00032746, /* Ebba */ 0x000b8dc3, 
    /* Edit */ 0x000fcc51, /* Eedla */ 0x000023c7, /* Eerika */ 0x00028414, /* Eeva */ 0x000a4c13, 
    /* Eevastiina */ 0x0009388e, /* Eevi */ 0x0001c421, /* Eija */ 0x0007e82a, /* Eijariina */ 0x000ba9ff, 
    /* Eila */ 0x000d4fac, /* Eine */ 0x0006e937, /* Eini */ 0x0000a51c, /* Eira */ 0x000c7073, 
    /* Eivor */ 0x0007f97c, /* Elena */ 0x000c64f8, /* Eleonoora */ 0x0008b2b2, /* Eleonora */ 0x00060c76, 
    /* Elida */ 0x00097516, /* Eliina */ 0x000dc2be, /* Eliisa */ 0x0001aea2, /* Elin */ 0x000e6493, 
    /* Elina */ 0x00069d9c, /* Elisa */ 0x000af180, /* Elisabet */ 0x000155f6, /* Elisabeth */ 0x0007cfde, 
    /* Elise */ 0x00073599, /* Elizabeth */ 0x00089e16, /* Ella */ 0x00068d47, /* Ellen */ 0x00069b2d, 
    /* Elli */ 0x000d0575, /* Elliida */ 0x00005fff, /* Ellinor */ 0x0005d16f, /* Elma */ 0x000dbc06, 
    /* Elmi */ 0x00063434, /* Elna */ 0x0000efc5, /* Elsa */ 0x000c83d9, /* Else */ 0x000147c0, 
    /* Elsi */ 0x00070beb, /* Elvi */ 0x0000ffae, /* Elviina */ 0x000f3856, /* Elviira */ 0x0008650b, 
    /* Elvira */ 0x0000d82a, /* Emili */ 0x00071049, /* Emilia */ 0x0007bf22, /* Emily */ 0x0000002d, 
    /* Emma */ 0x000fd631, /* Emmaliina */ 0x0006b068, /* Emmi */ 0x00045e03, /* Enna */ 0x00043bab, 
    /* Enni */ 0x000fb399, /* Ennika */ 0x000d5d22, /* Ennina */ 0x000aa967, /* Erica */ 0x000ec37d, 
    /* Eriikka */ 0x00007bc5, /* Erika */ 0x00074975, /* Erja */ 0x0004a4bb, /* Erna */ 0x000861bf, 
    /* Essi */ 0x000defa6, /* Ester */ 0x00087263, /* Esteri */ 0x000861a4, /* Estrid */ 0x00066376, 
    /* Etel */ 0x0000b87b, /* Eva */ 0x0005e212, /* Eveliina */ 0x0005ac70, /* Evilda */ 0x000967fc, 
    /* Fan */ 0x000fc54c, /* Fanni */ 0x0008a126, /* Fanny */ 0x000fb142, /* Fiia */ 0x000f1407, 
    /* Filippa */ 0x000c9b7b, /* Fiona */ 0x00056bcc, /* Floora */ 0x0008ce23, /* Flora */ 0x000cc6a3, 
    /* Fredrika */ 0x000ae09a, /* Freja */ 0x0006fb80, /* Frida */ 0x000f2f6a, /* Gabriella */ 0x000e1ddc, 
    /* Gerd */ 0x0005b513, /* Gerda */ 0x0002256d, /* Gertrud */ 0x000322b1, /* G?ta */ 0x0005c441, 
    /* Greta */ 0x0007edef, /* Gretel */ 0x000860bb, /* Gudrun */ 0x0005d401, /* Gun */ 0x0003782e, 
    /* Gunborg */ 0x0001a5b4, /* Gunhild */ 0x00095707, /* Gunilla */ 0x00040606, /* Gunnel */ 0x00081d0c, 
    /* Gunnevi */ 0x000477d9, /* Gunvor */ 0x000d3a2d, /* Gurli */ 0x000970c8, /* Gustava */ 0x0002b3ff, 
    /* Hagar */ 0x0004f1eb, /* Hanna */ 0x00039775, /* Hannamari */ 0x00053d09, /* Hanne */ 0x000e536c, 
    /* Hannele */ 0x0000f14d, /* Hanni */ 0x00081f47, /* Hannuntyt?r */ 0x000f772c, /* Harriet */ 0x00070136, 
    /* Hedda */ 0x00096d7e, /* Hedvig */ 0x0001bc73, /* Heidi */ 0x000a761f, /* Heini */ 0x00059e95, 
    /* Helen */ 0x000e1016, /* Helena */ 0x0007ba47, /* Helga */ 0x00076f05, /* Heli */ 0x0006e627, 
    /* Helin? */ 0x000b5f1a, /* Helj? */ 0x000fb79d, /* Helka */ 0x00022009, /* Hell? */ 0x000c12a8, 
    /* Helle */ 0x000e72d7, /* Hellen */ 0x000d4592, /* Hellevi */ 0x00039e40, /* Helli */ 0x00083efc, 
    /* Hellin */ 0x00080a9e, /* Helmi */ 0x00030fbd, /* Helmiina */ 0x0009478a, /* Helvi */ 0x0005c427, 
    /* Helvig */ 0x0005949c, /* Hely */ 0x0001f643, /* Henna */ 0x00010022, /* Henni */ 0x000a8810, 
    /* Henrietta */ 0x0003dc12, /* Henriikka */ 0x00058979, /* Henrika */ 0x0008f0c6, /* Hertta */ 0x0008a977, 
    /* Heta */ 0x0006f64c, /* Hilda */ 0x000c837e, /* Hildegard */ 0x00082eea, /* Hildegerd */ 0x00018636, 
    /* Hildur */ 0x000db735, /* Hilja */ 0x000faef0, /* Hilkka */ 0x000e8e09, /* Hilla */ 0x00050976, 
    /* Hille */ 0x0008cd6f, /* Hillevi */ 0x00015e3b, /* Hilma */ 0x000e3837, /* Hilppa */ 0x000b0902, 
    /* Hj?rdis */ 0x000f8083, /* Ida */ 0x000b6ba5, /* Iia */ 0x000515e8, /* Iida */ 0x00027a1c, 
    /* Iina */ 0x000d9296, /* Iines */ 0x000a3ea9, /* Iiris */ 0x00032bb1, /* Iita */ 0x0000684d, 
    /* Illusia */ 0x00008547, /* Ilma */ 0x000b03be, /* Ilmatar */ 0x0003b16c, /* Ilmi */ 0x00008b8c, 
    /* Ilona */ 0x000b0c2f, /* Ilse */ 0x0007f878, /* Ilta */ 0x000baaa6, /* Immi */ 0x0002e1bb, 
    /* Impi */ 0x000e8da7, /* Ina */ 0x0004832f, /* Inari */ 0x00003cc1, /* Ines */ 0x000f2c90, 
    /* Inga */ 0x00003f5a, /* Ingeborg */ 0x000bab37, /* Ingegerd */ 0x000b8f69, /* Ingela */ 0x000cbd70, 
    /* Inger */ 0x000b0d09, /* Ingrid */ 0x0008084f, /* Inka */ 0x00057056, /* Inkeri */ 0x0000b525, 
    /* Inna */ 0x00028413, /* Ira */ 0x0003de72, /* Irene */ 0x00076d4c, /* Irina */ 0x00005031, 
    /* Irinja */ 0x000cf8a7, /* Iris */ 0x00063988, /* Irja */ 0x00021b03, /* Irjaliisa */ 0x00023269, 
    /* Irma */ 0x00038dc4, /* Irmeli */ 0x000aafa5, /* Irmelin */ 0x0000615f, /* Iro */ 0x000bf375, 
    /* Isa */ 0x0008ef33, /* Isabella */ 0x0008753e, /* Jaana */ 0x000f8328, /* Jade */ 0x00094053, 
    /* Jadessa */ 0x00026a81, /* Jane */ 0x0006a8d9, /* Janette */ 0x0001367b, /* Janica */ 0x0002f22e, 
    /* Janika */ 0x000b7826, /* Janina */ 0x000c8c63, /* Janita */ 0x000176b8, /* Janna */ 0x0003c415, 
    /* Jannika */ 0x00094bde, /* Jasmiina */ 0x0003d4aa, /* Jasmin */ 0x000820ab, /* Jasmine */ 0x0006875f, 
    /* Jeanette */ 0x000a98a0, /* Jemina */ 0x0009854d, /* Jenika */ 0x000bdee6, /* Jenina */ 0x000c2aa3, 
    /* Jenna */ 0x00015342, /* Jenni */ 0x000adb70, /* Jennica */ 0x000183c0, /* Jennifer */ 0x00024404, 
    /* Jenniina */ 0x00049b2c, /* Jennina */ 0x000ffd8d, /* Jenny */ 0x000dcb14, /* Jessica */ 0x00025fb1, 
    /* Jetta */ 0x000d8f3f, /* Joanna */ 0x000034d1, /* Joanne */ 0x000df0c8, /* Johanna */ 0x00082d98, 
    /* Jonna */ 0x000cb326, /* Josefiina */ 0x000ac5e0, /* Josefin */ 0x0007c6b2, /* Josefina */ 0x000ff460, 
    /* Judit */ 0x0003f327, /* Julia */ 0x000d4674, /* Juliaana */ 0x000503ae, /* Juliana */ 0x000dc577, 
    /* Julianna */ 0x00094493, /* Julianne */ 0x0004808a, /* Julie */ 0x0000826d, /* Juliette */ 0x000acba0, 
    /* Jutta */ 0x0004d8a0, /* Juttamari */ 0x0001c823, /* Juulia */ 0x000e3993, /* Kaariina */ 0x000777f7, 
    /* Kaarina */ 0x0006cbd9, /* Kaija */ 0x00003e24, /* Kaino */ 0x0004d627, /* Kaisa */ 0x0000973c, 
    /* Kaisla */ 0x0000235b, /* Kaisu */ 0x000a4341, /* Kajsa */ 0x00062965, /* Kanerva */ 0x0000e7e6, 
    /* Karelia */ 0x00021d81, /* Karin */ 0x00013ce7, /* Karita */ 0x00024d3a, /* Karla */ 0x0009d533, 
    /* Karoliina */ 0x0007f3e2, /* Karolina */ 0x000ba4c3, /* Kaste */ 0x000de344, /* Kastehelmi */ 0x000ed99b, 
    /* Katariina */ 0x000d8e4b, /* Katarina */ 0x0004d913, /* Kati */ 0x0001794c, /* Katinka */ 0x0006e175, 
    /* Katja */ 0x000e0e07, /* Katri */ 0x000e1e6c, /* Katriina */ 0x000ce4df, /* Katrin */ 0x00092ffa, 
    /* Katta */ 0x000f31d8, /* Kerstin */ 0x000bb88d, /* Kerttu */ 0x00060fa4, /* Kerttuli */ 0x0008e997, 
    /* Kia */ 0x0001c186, /* Kiamilla */ 0x000599aa, /* Kielo */ 0x000c652e, /* Kiia */ 0x0005ccda, 
    /* Kiira */ 0x000f8e92, /* Kira */ 0x00030740, /* Kirsi */ 0x000c7b70, /* Kirsti */ 0x0008a86f, 
    /* Klaara */ 0x000787c4, /* Klara */ 0x00022f18, /* Krista */ 0x000b5650, /* Kristel */ 0x0003021d, 
    /* Kristiina */ 0x000e13bd, /* Kristina */ 0x0001d53b, /* Kristine */ 0x000c1122, /* Kukka */ 0x00041bc3, 
    /* Kukka-Maaria */ 0x0007882d, /* Kylli */ 0x0007ac0b, /* Kyllikki */ 0x0000377a, /* Lahja */ 0x00028803, 
    /* Laila */ 0x000a45b2, /* Laina */ 0x000c2730, /* Larissa */ 0x00048221, /* Lauha */ 0x000adaa2, 
    /* Laura */ 0x00072079, /* Lauriina */ 0x00053e63, /* Lea */ 0x000b980f, /* Leea */ 0x000d420b, 
    /* Leena */ 0x00044903, /* Leeni */ 0x000fc131, /* Leila */ 0x0008d2e5, /* Lemmikki */ 0x000732cc, 
    /* Lempi */ 0x000daf56, /* Lena */ 0x00099bc0, /* Lenita */ 0x00083365, /* Leonora */ 0x0005c0e8, 
    /* Lia */ 0x000ed703, /* Liina */ 0x00080fdf, /* Liisa */ 0x000463c3, /* Liisi */ 0x000febf1, 
    /* Lilian */ 0x000bed6a, /* Lilja */ 0x000f0830, /* Lilla */ 0x0005afb6, /* Lilli */ 0x000e2784, 
    /* Lina */ 0x000362a4, /* Linda */ 0x0008f1d0, /* Linn */ 0x000c7f35, /* Linna */ 0x0007195a, 
    /* Linnea */ 0x000eebf1, /* Lisa */ 0x000f0eb8, /* Lisbet */ 0x000a793c, /* Lisen */ 0x0002ed23, 
    /* Lotta */ 0x00009afb, /* Lotten */ 0x0004929d, /* Louise */ 0x000ca984, /* Loviisa */ 0x000f6337, 
    /* Lovisa */ 0x0004c273, /* Lucia */ 0x0001f4e9, /* Lulu */ 0x00038e4f, /* Lumi */ 0x0009e341, 
    /* Luna */ 0x000f38b0, /* Lydia */ 0x00085dd4, /* Lyydia */ 0x0002d98f, /* Lyyli */ 0x000a1180, 
    /* Maaret */ 0x0000dd3c, /* Maari */ 0x00038a57, /* Maaria */ 0x000876db, /* Maarit */ 0x00059230, 
    /* Magdalena */ 0x00080425, /* Magna */ 0x0002238a, /* Magnhild */ 0x00011686, /* Maia */ 0x000dc2be, 
    /* Maija */ 0x0000cb84, /* Maiju */ 0x000a1ff9, /* Maikki */ 0x00062c7b, /* Maila */ 0x000a6c02, 
    /* Maili */ 0x0001e430, /* Mailis */ 0x000da106, /* Maini */ 0x000786b2, /* Maire */ 0x000697c4, 
    /* Maisa */ 0x0000629c, /* Maj */ 0x0007eeb4, /* Maja */ 0x0000917d, /* Malin */ 0x0009473d, 
    /* Malina */ 0x00007450, /* Malla */ 0x0001aee9, /* Manta */ 0x000ee2de, /* Margareeta */ 0x0007f881, 
    /* Margareta */ 0x00011f95, /* Margit */ 0x00040bda, /* Mari */ 0x00008116, /* Maria */ 0x000ed4d6, 
    /* Marianna */ 0x000196bc, /* Marianne */ 0x000c52a5, /* Marie */ 0x000310cf, /* Mariitta */ 0x0009622e, 
    /* Marika */ 0x0001a0b9, /* Marilla */ 0x00036346, /* Marina */ 0x000654fc, /* Marinka */ 0x000621ef, 
    /* Marintyt?r */ 0x000dc031, /* Marit */ 0x0003303d, /* M?rit */ 0x00002d5f, /* Marita */ 0x000bae27, 
    /* Maritta */ 0x000d09d7, /* Marja */ 0x00038715, /* Marjaana */ 0x000dab51, /* Marjaliisa */ 0x000d60b9, 
    /* Marjatta */ 0x000d3011, /* Marjetta */ 0x000fa746, /* Marjo */ 0x000baa12, /* Marjukka */ 0x00061a0a, 
    /* Marjut */ 0x000bc5d4, /* Marketta */ 0x000f8ef6, /* Marleena */ 0x0006617a, /* Marlene */ 0x00070e60, 
    /* Marta */ 0x0002b8ca, /* M?rta */ 0x00002d5f, /* Martina */ 0x000d4c6d, /* Martta */ 0x00059e04, 
    /* Mary */ 0x00079172, /* Mataleena */ 0x000b98fe, /* Matilda */ 0x000a1cee, /* Matleena */ 0x000f8267, 
    /* Mea */ 0x0009f238, /* Meea */ 0x0001256e, /* Meeri */ 0x0008b5dc, /* Meiju */ 0x000888ae, 
    /* Melia */ 0x0004cdfb, /* Melike */ 0x000ae2cc, /* Melina */ 0x0000d290, /* Melinda */ 0x000f56d5, 
    /* Melissa */ 0x0002e260, /* Menna */ 0x00018f52, /* Meri */ 0x000929ca, /* Merja */ 0x00011042, 
    /* Mervi */ 0x000dc52d, /* Mette */ 0x00009736, /* Mia */ 0x000cbd34, /* Micaela */ 0x00054c79, 
    /* Michaela */ 0x0007ad36, /* Michaelsdotter */ 0x00001bc6, /* Mielikki */ 0x000ef3fb, /* Miia */ 0x000e9306, 
    /* Miina */ 0x0008266f, /* Miisa */ 0x00044a73, /* Mikaela */ 0x000507b8, /* Mila */ 0x00096743, 
    /* Milena */ 0x000ac6f5, /* Mili */ 0x0002ef71, /* Milja */ 0x000f2180, /* Miljaana */ 0x000e86dd, 
    /* Milka */ 0x000410c1, /* Milla */ 0x00058606, /* Milli */ 0x000e0e34, /* Mimi */ 0x0009de30, 
    /* Mimmi */ 0x00075542, /* Mimosa */ 0x000d485a, /* Minea */ 0x0003e921, /* Minella */ 0x00062410, 
    /* Minja */ 0x000bf5ee, /* Minka */ 0x0000c4af, /* Minna */ 0x000730ea, /* Minnea */ 0x00023854, 
    /* Minttu */ 0x0000e99f, /* Mira */ 0x0008589c, /* Miranda */ 0x000997a2, /* Mireile */ 0x00005861, 
    /* Mirella */ 0x00065e93, /* Mirelle */ 0x000b9a8a, /* Miria */ 0x000afc39, /* Mirja */ 0x0007affa, 
    /* Mirjam */ 0x000ef180, /* Mirjami */ 0x000d64e5, /* Mirka */ 0x000c9ebb, /* Mirkka */ 0x00053f16, 
    /* Mirva */ 0x0000f2a7, /* Mona */ 0x00027973, /* Monika */ 0x000ef6ff, /* Monna */ 0x000c6f36, 
    /* My */ 0x00063518, /* Nadja */ 0x00082207, /* Nana */ 0x0009fb97, /* Nanna */ 0x000362d5, 
    /* Nanne */ 0x000ea6cc, /* Nanny */ 0x000ffa83, /* Naomi */ 0x0007d313, /* Natalia */ 0x000850d1, 
    /* Natalie */ 0x000594c8, /* Nea */ 0x000f4c61, /* Neea */ 0x00048a80, /* Neela */ 0x000278e1, 
    /* Nella */ 0x0003436e, /* Nelli */ 0x0008cb5c, /* Netta */ 0x000d29ff, /* Nia */ 0x000a036d, 
    /* Niina */ 0x00085cbf, /* Nina */ 0x000aaa2f, /* Ninna */ 0x00074a3a, /* Ninni */ 0x000cc208, 
    /* Nita */ 0x000750f4, 
    /* Nonet */ 0x000528c6, /* Nonett */ 0x00079740, /* Nonette */ 0x00047318,  
    /* Noona */ 0x000e7fd1, /* Noora */ 0x0009228c, /* Nora */ 0x00008bc0, 
    /* Oili */ 0x000b27fa, /* Oivi */ 0x0006dd21, /* Olga */ 0x000fb4e8, /* Olivia */ 0x000d9352, 
    /* Onerva */ 0x0008d9c8, /* Onika */ 0x0008b9f3, /* Oona */ 0x000bb1f8, /* Orvokki */ 0x0000f336, 
    /* Otteljaana */ 0x000d3de1, /* Outa */ 0x00076d85, /* Outi */ 0x000ce5b7, /* P?iv? */ 0x00064186, 
    /* P?ivi */ 0x00064186, /* P?ivikki */ 0x00064186, /* P?lvi */ 0x00064186, /* Pamela */ 0x00089996, 
    /* Patricia */ 0x000bdab3, /* Paula */ 0x00066525, /* Pauliina */ 0x000133e4, /* Paulina */ 0x00091203, 
    /* Peppiina */ 0x0003fcc7, /* Peppina */ 0x00073742, /* Pernilla */ 0x0006d446, /* Petra */ 0x0007a79a, 
    /* Pia */ 0x00028d17, /* Pihla */ 0x000c7de9, /* Piia */ 0x000d1c44, /* Pilvi */ 0x0003a7dc, 
    /* Pinja */ 0x000ba6dd, /* Pinjaliina */ 0x000d8081, /* Pirita */ 0x0003f4a3, /* Piritta */ 0x000776b4, 
    /* Pirjo */ 0x000fd1ce, /* Pirkko */ 0x00050354, /* Pulmu */ 0x000bd82e, /* Raakel */ 0x000b6ddb, 
    /* Ragna */ 0x000223d9, /* Ragnborg */ 0x0002c7b2, /* Ragnhild */ 0x000a3501, /* Ragni */ 0x0009abeb, 
    /* Raija */ 0x0000cbd7, /* Raila */ 0x000a6c51, /* Raili */ 0x0001e463, /* Raisa */ 0x000062cf, 
    /* Raita */ 0x0001f408, /* Rakel */ 0x000d7fcb, /* Ramona */ 0x0001dfc9, /* Rauha */ 0x000af341, 
    /* Rauna */ 0x000054c7, /* Rauni */ 0x000bdcf5, /* Rea */ 0x00031675, /* Rebecca */ 0x0006bebf, 
    /* Rebecka */ 0x000f34b7, /* Rebekka */ 0x000c650f, /* Reea */ 0x000b62a7, /* Reeta */ 0x00099a3b, 
    /* Reetta */ 0x000be7ac, /* Regina */ 0x0001e5df, /* Regine */ 0x000c21c6, /* Reija */ 0x00025c80, 
    /* Rianna */ 0x00054187, /* Rigmor */ 0x000dd09d, /* Riia */ 0x0004d4cf, /* Riikka */ 0x000d5fc6, 
    /* Riikkaliisa */ 0x0002fe10, /* Riina */ 0x0008263c, /* Riitta */ 0x000db515, /* Ristiina */ 0x000245a0, 
    /* Rita */ 0x0008b8d3, /* Ritva */ 0x000d8e46, /* Ronja */ 0x0000aa61, /* Roosa */ 0x0002694e, 
    /* Rosa */ 0x000452a6, /* Runa */ 0x0009181c, /* Rut */ 0x000ce0cf, /* Ruusu */ 0x000d2c6e, 
    /* Ruut */ 0x0002376d, /* Saaga */ 0x000dcd92, /* Saana */ 0x000f76db, /* Saara */ 0x00082b86, 
    /* Sabina */ 0x00036088, /* Sade */ 0x00085846, /* S?de */ 0x000f103c, /* Saga */ 0x0008cf9c, 
    /* Saija */ 0x0000e267, /* Saila */ 0x000a45e1, /* Saima */ 0x000174a0, /* Saimi */ 0x000afc92, 
    /* Saini */ 0x0007af51, /* Salla */ 0x0001870a, /* Salli */ 0x000a0f38, /* Sally */ 0x000d1f5c, 
    /* Salme */ 0x00077252, /* Sandra */ 0x000a113e, /* Sanelma */ 0x000f70af, /* Sani */ 0x0001fce7, 
    /* Sanna */ 0x000331e6, /* Sanni */ 0x0008b9d4, /* Sara */ 0x000d2988, /* Sari */ 0x0006a1ba, 
    /* Sarita */ 0x0007cdcc, /* Sarlotta */ 0x0003919b, /* Satu */ 0x000d5a73, /* Satumarja */ 0x00098f86, 
    /* Seena */ 0x00044950, /* Seidi */ 0x000ad08c, /* Seija */ 0x00027530, /* Selena */ 0x0006481f, 
    /* Selina */ 0x000cb17b, /* Selja */ 0x0009b7db, /* Selma */ 0x0008211c, /* Senja */ 0x000d63b5, 
    /* Senni */ 0x000a2e83, /* Serafia */ 0x00079fbf, /* Serafiina */ 0x000f64a0, /* Serena */ 0x000068b3, 
    /* Signe */ 0x000be69f, /* Signhild */ 0x000f7bf8, /* Sigrid */ 0x0005f5a2, /* Siina */ 0x00080f8c, 
    /* Siiri */ 0x0004dae3, /* Silja */ 0x000f0863, /* Silke */ 0x0009fd3b, /* Silva */ 0x0008553e, 
    /* Silvia */ 0x00072ef0, /* Sina */ 0x0009256d, /* Sini */ 0x0002ad5f, /* Sinikka */ 0x000ffc79, 
    /* Sinna */ 0x00071909, /* Sirena */ 0x000085b2, /* Siri */ 0x0005f002, /* Sirja */ 0x00078619, 
    /* Sirke */ 0x00017341, /* Sirkka */ 0x00095cfd, /* Sirkku */ 0x00038880, /* Sirpa */ 0x000a7cc2, 
    /* Siru */ 0x0004ac4d, /* Sisko */ 0x0006f068, /* Sissi */ 0x000ecd04, /* Siv */ 0x0007b689, 
    /* Sivi */ 0x00093506, /* Sofia */ 0x000e81aa, /* Sofiina */ 0x0005748e, /* Sohvi */ 0x00012a0c, 
    /* Soila */ 0x000532d2, /* Soile */ 0x0008f6cb, /* Soili */ 0x000ebae0, /* Sointu */ 0x000a61cb, 
    /* Soja */ 0x00089cdb, /* Solja */ 0x000457bf, /* Solveig */ 0x0007e170, /* Sonja */ 0x000083d1, 
    /* Sorja */ 0x000cd9c5, /* Stella */ 0x0009e0aa, /* Stiina */ 0x0002ff7b, /* Stina */ 0x000b80ce, 
    /* Suoma */ 0x0007c8da, /* Suometar */ 0x000c561d, /* Susanna */ 0x000373eb, /* Susanne */ 0x000eb7f2, 
    /* Suvi */ 0x00056f12, /* Svea */ 0x000718eb, /* Sylvi */ 0x000a8a93, /* Sylvia */ 0x0007b972, 
    /* Synn?ve */ 0x00009b8c, /* Taija */ 0x00003e77, /* Taika */ 0x000b0f36, /* Taikatuuli */ 0x0002048e, 
    /* Taimi */ 0x000a2082, /* Taina */ 0x000cfb73, /* Taisa */ 0x0000976f, /* Talvikki */ 0x000a4977, 
    /* Tamara */ 0x00014c83, /* Tanja */ 0x000f28f2, /* Tara */ 0x000a1131, /* Tarika */ 0x0008f3ea, 
    /* Tarja */ 0x000372e6, /* Taru */ 0x0000c54c, /* Tea */ 0x000e6ac7, /* Teea */ 0x00003d7b, 
    /* Teija */ 0x0002a920, /* Tekla */ 0x000cdac8, /* Tellervo */ 0x000b0c29, /* Teresa */ 0x00093417, 
    /* Terese */ 0x0004f00e, /* Teresia */ 0x000f57fd, /* Terhi */ 0x000c0f01, /* Terhikki */ 0x0009276c, 
    /* Terttu */ 0x0006bfea, /* Tessa */ 0x0003269e, /* Tia */ 0x000b25cb, /* Tianna */ 0x000ca29a, 
    /* Tiia */ 0x000f8b13, /* Tiiatuulia */ 0x000aa7c8, /* Tiina */ 0x0008d39c, /* Tiitu */ 0x000ffd3a, 
    /* Tiiu */ 0x00055f6e, /* Tilda */ 0x000cf9fd, /* Tina */ 0x000e1dd4, /* Tinja */ 0x000b001d, 
    /* Tinka */ 0x0000315c, /* Titta */ 0x000b1964, /* Toini */ 0x00080472, /* Tora */ 0x00043c3b, 
    /* Torborg */ 0x000a7899, /* Torhild */ 0x00028a2a, /* Tove */ 0x00053d26, /* Trine */ 0x000dc71b, 
    /* Trude */ 0x000e7585, /* Tua */ 0x000c7896, /* Tuija */ 0x000bfebf, /* Tuikku */ 0x000e1225, 
    /* Tuire */ 0x000da2ff, /* Tuomi */ 0x000c9cf8, /* Tuovi */ 0x000a5762, /* Tuula */ 0x000d032d, 
    /* Tuuli */ 0x00068b1f, /* Tuulia */ 0x00025a78, /* Tuulikki */ 0x00022dda, /* Tuutikki */ 0x0002f199, 
    /* Tyra */ 0x000fe3f9, /* Tytti */ 0x0009c6c9, /* Tyyne */ 0x000ae36a, /* Tyyni */ 0x000caf41, 
    /* Ulla */ 0x000fdad8, /* Ulpu */ 0x000253f8, /* Ulrika */ 0x0004e4fe, /* Unelma */ 0x000bbdd5, 
    /* Urda */ 0x000edeaa, /* Ursula */ 0x000566ab, /* Valborg */ 0x000f2123, /* Valma */ 0x000a393b, 
    /* Valpuri */ 0x0006b596, /* Vanamo */ 0x000e61ff, /* Vanessa */ 0x000c849a, /* Vappu */ 0x0000db4e, 
    /* Varma */ 0x0002b741, /* Varpu */ 0x00040f20, /* Vaula */ 0x00069085, /* Veea */ 0x0009f5f0, 
    /* Veera */ 0x00039b7d, /* Vellamo */ 0x000ebfa0, /* Vendla */ 0x00061992, /* Venla */ 0x00074b43, 
    /* Vera */ 0x000a7166, /* Verna */ 0x000d73d5, /* Veronika */ 0x000498e4, /* Vesta */ 0x0002e339, 
    /* Viena */ 0x00027998, /* Vieno */ 0x000a549f, /* Viivi */ 0x00089097, /* Viktoria */ 0x00044e26, 
    /* Vilhelmiina */ 0x000ee211, /* Vilhelmina */ 0x000a33d3, /* Vilja */ 0x000f8713, /* Vilma */ 0x000e11d4, 
    /* Viola */ 0x00039ecc, /* Virpi */ 0x00017b80, /* Virva */ 0x00005434, /* Virve */ 0x000d902d, 
    /* Vivan */ 0x000565ef, /* Viveka */ 0x00087713, /* Vivi */ 0x0007c534, /* Vivian */ 0x00027b6c, 
    /* Viviana */ 0x00077e0e, /* Vuokko */ 0x000f1588, /* Wilhelmiina */ 0x00003dd1, /* Wilma */ 0x000e3864, 
    /* Yanna */ 0x00032947, /* Ylva */ 0x00049fbb, /* Yrsa */ 0x000be584, /* Yvonne */ 0x0000023d
};

unsigned int men_names[NUM_MEN] = {
    /* Aadolf */ 0x00091757, /* Aake */ 0x0000db9d, /* Aapeli */ 0x000a4fd8, /* Aapo */ 0x0003f919, 
    /* Aappo */ 0x000269a6, /* Aaretti */ 0x00005429, /* Aarne */ 0x00026b09, /* Aarni */ 0x00042722, 
    /* Aarno */ 0x00078217, /* Aaro */ 0x00059b9b, /* Aaron */ 0x000b83c0, /* Aarre */ 0x00053654, 
    /* Aarto */ 0x000a78cc, /* Aatami */ 0x000a4112, /* Aatos */ 0x000093ab, /* Aatto */ 0x0007047e, 
    /* Aatu */ 0x000dc567, /* Abel */ 0x0009f0ee, /* Abraham */ 0x0002c9c9, /* Adam */ 0x000f79ce, 
    /* Adolf */ 0x000d9820, /* Adrian */ 0x00031aa9, /* Ahti */ 0x000da2a7, /* Ahto */ 0x000e0792, 
    /* Ahvo */ 0x00086510, /* Aimo */ 0x000cc4bd, /* ?ke */ 0x000fffff, /* Aki */ 0x000baee0, 
    /* Akseli */ 0x000ff897, /* Aku */ 0x000af2af, /* Akusti */ 0x0004e0d0, /* Alarik */ 0x000f9b0f, 
    /* Albert */ 0x000d477b, /* Albin */ 0x000d5feb, /* Ale */ 0x000c740c, /* Aleksander */ 0x00006c24, 
    /* Aleksanteri */ 0x00032a16, /* Aleksi */ 0x00090f50, /* Aleksis */ 0x0000f8b5, /* Aleksius */ 0x000ab033, 
    /* Alex */ 0x000d0999, /* Alexander */ 0x00030b80, /* Alexis */ 0x000611d8, /* Alf */ 0x000525b6, 
    /* Alfons */ 0x000b9d04, /* Alfred */ 0x0002f12b, /* Algot */ 0x000ec3fc, /* Ali */ 0x000a3827, 
    /* Allan */ 0x000af8e9, /* Alpi */ 0x0008cf7f, /* Alpo */ 0x000b6a4a, /* Altti */ 0x000e7996, 
    /* Alvar */ 0x000a8200, /* Alveri */ 0x0000eb6a, /* Alvi */ 0x000268f9, /* Anders */ 0x0002de64, 
    /* Andr? */ 0x000705a8, /* Andreas */ 0x000b3dd1, /* Andreevits */ 0x000e25c2, /* Ano */ 0x000fff90, 
    /* Anselmi */ 0x0003949c, /* Anssi */ 0x0009315f, /* Ante */ 0x0006923e, /* Antero */ 0x000ad5b4, 
    /* Anthony */ 0x00056daf, /* Anton */ 0x0005ef24, /* Antti */ 0x0007b11d, /* Antto */ 0x00041428, 
    /* Anttoni */ 0x000d95ec, /* Arhippa */ 0x000af16d, /* Arho */ 0x00087c69, /* Ari */ 0x000b07f8, 
    /* Armas */ 0x0008deab, /* Arne */ 0x000732f1, /* Arno */ 0x0002dbef, /* Arnold */ 0x000a7b4d, 
    /* Arsi */ 0x000d12c6, /* Arto */ 0x000f2134, /* Artti */ 0x0008593a, /* Arttu */ 0x00090575, 
    /* Artturi */ 0x000f2cf7, /* Artur */ 0x0006a197, /* Arvi */ 0x000ae683, /* Arvid */ 0x00019df5, 
    /* Arvo */ 0x000943b6, /* Asko */ 0x0007459d, /* Aslak */ 0x000a4baf, /* Asmo */ 0x000de21b, 
    /* Asser */ 0x0007c226, /* Atle */ 0x000c2cc1, /* Atro */ 0x0008fa00, /* Atso */ 0x0003cb41, 
    /* Atte */ 0x0007b498, /* August */ 0x000ed08b, /* Augusti */ 0x00014d4c, /* Augustinus */ 0x0002ffba, 
    /* Aukusti */ 0x0001a04d, /* Aulis */ 0x0004062d, /* Auno */ 0x000dcd6a, /* Auvo */ 0x00065533, 
    /* Axel */ 0x0008d648, /* Ben */ 0x000aa894, /* Bengt */ 0x000e4721, /* Benjam */ 0x00074673, 
    /* Benjami */ 0x0007aef4, /* Benjamin */ 0x00022d3c, /* Benny */ 0x000d80d5, /* Bernhard */ 0x0007dd76, 
    /* Bernt */ 0x0000a67c, /* Bertel */ 0x000bd5bc, /* Bertil */ 0x000e9ab0, /* Birger */ 0x000418f7, 
    /* Bjarne */ 0x0002a7c1, /* Bj?rn */ 0x00006809, /* Bo */ 0x000a9c86, /* Boris */ 0x0008457c, 
    /* B?rje */ 0x000f30ce, /* Bror */ 0x000a2999, /* Bruno */ 0x00041633, /* Brynolf */ 0x000c41c8, 
    /* Caj */ 0x0009c3be, /* Carl */ 0x000502aa, /* Caspian */ 0x00078889, /* Christian */ 0x000d2783, 
    /* Christoff */ 0x000a45c4, /* Christoffer */ 0x000bcdc7, /* Conny */ 0x00004901, /* Daavid */ 0x00047b40, 
    /* Dag */ 0x0007a986, /* Dan */ 0x000b1122, /* Dani */ 0x000f93c1, /* Daniel */ 0x00034865, 
    /* David */ 0x000deaf4, /* Deniz */ 0x0005b208, /* Dennis */ 0x0001bad9, /* Dick */ 0x000cdd18, 
    /* Dimitri */ 0x0007e18e, /* Ebbe */ 0x000649da, /* Edgar */ 0x000ac568, /* Edvard */ 0x000936dc, 
    /* Edvin */ 0x0006da68, /* Edward */ 0x000551b9, /* Eeli */ 0x000c3efa, /* Eelis */ 0x000df772, 
    /* Eemeli */ 0x0008244c, /* Eemil */ 0x000790b0, /* Eerik */ 0x0009e15e, /* Eerikki */ 0x00086747, 
    /* Eero */ 0x000ea410, /* Eetu */ 0x0006faec, /* Eevert */ 0x0007a7d4, /* Egil */ 0x0005ea5e, 
    /* Eikka */ 0x000b4d85, /* Einar */ 0x0001263a, /* Einari */ 0x0005f1a0, /* Eino */ 0x00030029, 
    /* Ejvind */ 0x000d8458, /* Elia */ 0x00017902, /* Elias */ 0x0007f01b, /* Eliel */ 0x000338ea, 
    /* Elijah */ 0x000fd88c, /* Elis */ 0x0008084a, /* Elja */ 0x000c2ac1, /* Eljas */ 0x00014e42, 
    /* Elmer */ 0x0005ad55, /* Elmeri */ 0x000979e2, /* Elmo */ 0x00059101, /* Elof */ 0x000f4b27, 
    /* Emanuel */ 0x000d67ba, /* Emil */ 0x00026f88, /* Emmanuel */ 0x0001e543, /* Enar */ 0x000266ba, 
    /* Engelbert */ 0x00096d00, /* Engelbrekt */ 0x000a58cd, /* Ensio */ 0x0007c871, /* Eric */ 0x00079654, 
    /* Erik */ 0x000c1e66, /* Erkki */ 0x00081529, /* Erkko */ 0x000bb01c, /* Erland */ 0x000abe58, 
    /* Ernesti */ 0x00026c09, /* Erno */ 0x00004cb8, /* Ernst */ 0x000e2342, /* Esa */ 0x00021657, 
    /* Esaias */ 0x00088785, /* Eskil */ 0x0008e441, /* Esko */ 0x0005d2ca, /* Eugen */ 0x00026cd9, 
    /* Evald */ 0x000dedd2, /* Evert */ 0x00026ab5, /* Fabian */ 0x000ff2fe, /* Felix */ 0x000f542a, 
    /* Ferdinand */ 0x000bf872, /* Filip */ 0x000263a0, /* Finn */ 0x00019f51, /* Fjalar */ 0x0000f2a5, 
    /* Folke */ 0x0002ba15, /* Frank */ 0x00067f46, /* Frans */ 0x000ae710, /* Fred */ 0x0003e315, 
    /* Fredrik */ 0x000647e7, /* Frej */ 0x000bce12, /* Frejvid */ 0x000f8599, /* Fridolf */ 0x000e123d, 
    /* Fritjof */ 0x0001d48a, /* Gabriel */ 0x00037e0b, /* Geir */ 0x0007cbd8, /* Georg */ 0x000705ac, 
    /* Gerhard */ 0x000d4178, /* Germund */ 0x000be7bb, /* Gert */ 0x0002a577, /* Glenn */ 0x00032a09, 
    /* G?ran */ 0x0005c441, /* G?sta */ 0x0005c441, /* G?te */ 0x0005c441, /* Gottfrid */ 0x000741e2, 
    /* Greger */ 0x000740f1, /* Gregorius */ 0x000a068f, /* Grels */ 0x000504fe, /* Gudmund */ 0x000d8702, 
    /* Gunnar */ 0x000be56b, /* Gustav */ 0x00048896, /* Guy */ 0x0000fde9, /* H?kan */ 0x000ad9d0, 
    /* Halvar */ 0x00015cbf, /* Hannes */ 0x0006936e, /* Hannu */ 0x00094308, /* Hans */ 0x000bd503, 
    /* Harald */ 0x000402fa, /* Harri */ 0x0003180e, /* Harry */ 0x0004086a, /* Heikki */ 0x000b1b08, 
    /* Heimo */ 0x000b6863, /* Heino */ 0x00063ba0, /* Helge */ 0x000aab1c, /* Helmer */ 0x000e73ea, 
    /* Hemmi */ 0x0001658a, /* Hemming */ 0x00069caf, /* Hemminki */ 0x0006fedd, /* Henning */ 0x00034991, 
    /* Henri */ 0x000dd54d, /* Henrik */ 0x0003b8e0, /* Henrikki */ 0x00094ecb, /* Henry */ 0x000ac529, 
    /* Herbert */ 0x0006694d, /* Herkko */ 0x00006ea3, /* Herman */ 0x0005ca0d, /* Hermanni */ 0x00059239, 
    /* Hilding */ 0x000c3aee, /* Hjalmar */ 0x0000f4e6, /* Holger */ 0x0009ee9d, /* Hugo */ 0x000739a9, 
    /* Iikka */ 0x000ba084, /* Iiro */ 0x0002e2cc, /* Iisakki */ 0x000ecc17, /* Iivari */ 0x000a4eab, 
    /* Iivo */ 0x000e27c8, /* Ilari */ 0x0009f44a, /* Ilja */ 0x000a9579, /* Ilkka */ 0x000550b6, 
    /* Ilmari */ 0x00021145, /* Ilmo */ 0x00032eb9, /* Ilppo */ 0x0008faba, /* Immanuel */ 0x000ef00c, 
    /* Immo */ 0x0001448e, /* Inge */ 0x000dfb43, /* Ingemar */ 0x000e076e, /* Ingmar */ 0x000fd35b, 
    /* Ingolf */ 0x000fad05, /* Ingvald */ 0x0002cd67, /* Ingvar */ 0x000c9fca, /* Into */ 0x000753cf, 
    /* Isak */ 0x000141e1, /* Isko */ 0x00036d72, /* Ismo */ 0x0009caf4, /* Isto */ 0x000963ec, 
    /* Ivar */ 0x00012bca, /* Jaakkima */ 0x000919d2, /* Jaakko */ 0x00049119, /* Jaakob */ 0x000928a0, 
    /* Jaakoppi */ 0x000bf254, /* Jael */ 0x000ec9b6, /* Jakob */ 0x000a6605, /* Jali */ 0x00068670, 
    /* Jalmari */ 0x0004c7c2, /* Jalo */ 0x00052345, /* Jami */ 0x000db731, /* Jan */ 0x00053c28, 
    /* Jani */ 0x0000e4f2, /* Janne */ 0x000e000c, /* Jari */ 0x0007b9af, /* Jarkko */ 0x00086968, 
    /* Jarl */ 0x000d4d20, /* Jarmo */ 0x000ae0c5, /* Jarno */ 0x0007b306, /* Jaro */ 0x00041c9a, 
    /* Jasper */ 0x000203cb, /* Jasperi */ 0x000ee00f, /* Jasu */ 0x000dd4a1, /* Jens */ 0x000bb554, 
    /* Jere */ 0x00085d58, /* Jeremia */ 0x00063e14, /* Jeremias */ 0x0003920d, /* Jerker */ 0x000d8eff, 
    /* Jermu */ 0x000a8ee8, /* Jesper */ 0x0002a50b, /* Jesse */ 0x000ecb64, /* Jim */ 0x0005e79a, 
    /* Jimi */ 0x000ee689, /* Jimmy */ 0x00009936, /* Jiri */ 0x0004e817, /* Jirko */ 0x00046fac, 
    /* Joakim */ 0x000c2cd6, /* Joel */ 0x0000e4bc, /* Johan */ 0x0006ceca, /* Johannes */ 0x00083611, 
    /* John */ 0x0000fbdd, /* Johnny */ 0x0004e30d, /* Jomi */ 0x00039a3b, /* Jon */ 0x000611a6, 
    /* Jonas */ 0x000ddea1, /* Jonatan */ 0x000effc0, /* Jone */ 0x000885d3, /* Joni */ 0x000ec9f8, 
    /* Jonne */ 0x0001773f, /* Jonni */ 0x00073b14, /* Jooa */ 0x000e708b, /* Joona */ 0x000ed911, 
    /* Joonas */ 0x000e2e65, /* Joonatan */ 0x00050e55, /* Joosef */ 0x00013fa9, /* Jooseppi */ 0x0003d45e, 
    /* J?ran */ 0x0004b8fc, /* J?rgen */ 0x0004b8fc, /* Jori */ 0x000994a5, /* Jorma */ 0x000dbaf1, 
    /* J?rn */ 0x0004b8fc, /* Jose */ 0x0004e9cf, /* Josef */ 0x0002cf6d, /* Jouko */ 0x000026f5, 
    /* Jouni */ 0x00047785, /* Jousia */ 0x000452fd, /* Juha */ 0x000ec0ea, /* Juhana */ 0x0004b345, 
    /* Juhani */ 0x000f3b77, /* Juho */ 0x0006eded, /* Jukka */ 0x00043273, /* Julius */ 0x000acf78, 
    /* Junior */ 0x0009cdbe, /* Juri */ 0x0008b203, /* Jusa */ 0x00080b70, /* Jussi */ 0x0001d0d0, 
    /* Justus */ 0x000eb892, /* Jusu */ 0x0002df0d, /* Juuso */ 0x000f0957, /* Jyri */ 0x00024b67, 
    /* Jyrki */ 0x000e9d06, /* Kaapo */ 0x0006b840, /* Kaappo */ 0x000dc5b7, /* Kaapro */ 0x000ba735, 
    /* Kaarle */ 0x000ba59a, /* Kaarlo */ 0x000e4c84, /* Kai */ 0x0003c3bc, /* Kaj */ 0x000a9206, 
    /* Kalervo */ 0x00089981, /* Kaleva */ 0x0008f670, /* Kalevi */ 0x00037e42, /* Kalle */ 0x000c9f50, 
    /* Kanervo */ 0x0008cae1, /* Kari */ 0x000bdeca, /* Karl */ 0x00012a45, /* Karo */ 0x00087bff, 
    /* Karri */ 0x000362de, /* Kasimir */ 0x0008a060, /* Kasper */ 0x000ed06e, /* Kasperi */ 0x0009ebbb, 
    /* Kauko */ 0x000f7876, /* Kauno */ 0x00088c33, /* Kauto */ 0x000576e8, /* Keimo */ 0x000b12b3, 
    /* Kennet */ 0x0006d2a3, /* Kent */ 0x00034792, /* Kerkko */ 0x00041c0d, /* Kettil */ 0x000a94a4, 
    /* Kim */ 0x00078dad, /* Kimi */ 0x000281ec, /* Kimmo */ 0x000405d7, /* Kjell */ 0x00009b7a, 
    /* Klas */ 0x000ef571, /* Klaus */ 0x000ac897, /* Knut */ 0x000063e9, /* Konrad */ 0x00014576, 
    /* Konsta */ 0x000c3dda, /* Konstantin */ 0x00038e5c, /* Kosti */ 0x0004d85c, /* Krister */ 0x000c3f7e, 
    /* Kristian */ 0x0006d465, /* Kristoffer */ 0x000bd0b0, /* Kuisma */ 0x000b2358, /* Kullervo */ 0x000eb360, 
    /* Kurt */ 0x0002b9bf, /* Kustaa */ 0x0005cd2a, /* Kustavi */ 0x000951b9, /* Ky??sti */ 0x000c929e, 
    /* Ky?sti */ 0x000c929e, /* Lari */ 0x000ce673, /* Lars */ 0x000e1f09, /* Lasse */ 0x000ca993, 
    /* Lassi */ 0x000ae5b8, /* Launo */ 0x00085023, /* Lauri */ 0x000ca84b, /* Laurinpoika */ 0x000cd4b6, 
    /* Leander */ 0x000d5420, /* Leevi */ 0x00045968, /* Leif */ 0x000c98a4, /* Lennart */ 0x000620b9, 
    /* Lenne */ 0x000c62fb, /* Lenni */ 0x000a2ed0, /* Leo */ 0x0003b508, /* Leonard */ 0x0001196d, 
    /* Leopold */ 0x00082b14, /* Linus */ 0x0008a388, /* Luca */ 0x000146fd, /* Lucas */ 0x00010fa9, 
    /* Ludvig */ 0x000069e7, /* Luka */ 0x0008ccf5, /* Lukas */ 0x00025e11, /* Luuka */ 0x000c49a9, 
    /* Luukas */ 0x0008374b, /* Lyly */ 0x000f3b00, /* Magnus */ 0x00015f06, /* Mainio */ 0x00082927, 
    /* Manne */ 0x000edc1c, /* M?ns */ 0x00002d5f, /* Manu */ 0x00068004, /* Markku */ 0x000fa0aa, 
    /* Marko */ 0x00009b53, /* Markonpoika */ 0x00077985, /* Markus */ 0x000d3a40, /* M?rten */ 0x00002d5f, 
    /* Martin */ 0x0006ef89, /* Martti */ 0x000e1636, /* Matias */ 0x000c66a7, /* Mats */ 0x0008dfea, 
    /* Matti */ 0x00044c4a, /* Mattias */ 0x000cd44a, /* Mauno */ 0x00087993, /* Maunu */ 0x000a80e9, 
    /* Mauri */ 0x000c81fb, /* Mauritz */ 0x000efc96, /* Max */ 0x000e9ffc, /* Mertsi */ 0x000f2631, 
    /* Mies */ 0x0002ad42, /* Miika */ 0x000fd22a, /* Miikael */ 0x0002db94, /* Miikka */ 0x000def88, 
    /* Miikkael */ 0x000b7bcc, /* Mika */ 0x0008f184, /* Mikael */ 0x0001f3e6, /* Mikko */ 0x00032b43, 
    /* Miko */ 0x0000dc83, /* Miray */ 0x000fee67, /* Mirko */ 0x0004b3bc, /* Miro */ 0x0000759b, 
    /* Miska */ 0x000ef48c, /* Mitja */ 0x000ad348, /* Mitro */ 0x00096616, /* Moritz */ 0x000ed9aa, 
    /* Nemo */ 0x00052d8f, /* Nestori */ 0x0005948b, /* Nicholas */ 0x0001e96c, /* Nicky */ 0x0004b57a, 
    /* Niclas */ 0x000df205, /* Nico */ 0x000cf965, /* Niilo */ 0x0006133a, /* Nikke */ 0x0006b88d, 
    /* Niklas */ 0x0009daea, /* Niko */ 0x0005736d, /* Nikodemus */ 0x00039272, /* Nikolai */ 0x0006a7e6, 
    /* Nikolas */ 0x00045e9c, /* Nils */ 0x0005b9e5, /* Nipa */ 0x000b95f0, /* Noel */ 0x000273eb, 
    /* Noeli */ 0x000410b7, /* Nuutti */ 0x000d6d63, /* Nyyrikki */ 0x000b83d5, /* Odin */ 0x0000d54f, 
    /* Ohto */ 0x000170a1, /* Oiva */ 0x000d5513, /* ?jvind */ 0x000fffff, /* Okko */ 0x000dc066, 
    /* Ola */ 0x000f9d1f, /* Olav */ 0x000696a9, /* Olavi */ 0x0005c4ee, /* Ole */ 0x00025906, 
    /* Oliver */ 0x00069d80, /* Olle */ 0x0006a93a, /* Olli */ 0x0000e511, /* Olof */ 0x0002ab43, 
    /* Onni */ 0x000253fd, /* Ora */ 0x000ea2c0, /* ?rjan */ 0x000fffff, /* Orvar */ 0x000c1ccd, 
    /* Orvo */ 0x00063485, /* Oskar */ 0x000e4b8b, /* Oskari */ 0x0000add7, /* Osmo */ 0x00029528, 
    /* Ossi */ 0x00000fc2, /* Ossian */ 0x0005572e, /* ?sten */ 0x000fffff, /* Osvald */ 0x000361f7, 
    /* Otso */ 0x000cbc72, /* Otto */ 0x000d2ab5, /* Ove */ 0x000fa3dd, /* Paavali */ 0x00032fb2, 
    /* Paavo */ 0x000cb955, /* P?ivi? */ 0x00064186, /* P?iv? */ 0x00064186, /* Panu */ 0x00050f46, 
    /* Pasi */ 0x00083f15, /* Patrick */ 0x00048c4c, /* Patrik */ 0x0002292d, /* Paul */ 0x00086c1c, 
    /* Pauli */ 0x000ded17, /* Paulus */ 0x000d0539, /* Peetu */ 0x00031d26, /* Peik */ 0x00020c3e, 
    /* Pekka */ 0x000deacf, /* Pekko */ 0x0005c7c8, /* Pellervo */ 0x000102d3, /* Pentti */ 0x00094994, 
    /* Per */ 0x000983c5, /* Pertti */ 0x0006a1b3, /* Perttu */ 0x0007fdfc, /* Peter */ 0x000a62d2, 
    /* Petja */ 0x000c3fc3, /* Petri */ 0x000c2fa8, /* Petrik */ 0x00028fed, /* Petro */ 0x000f8a9d, 
    /* Petrus */ 0x00094ae6, /* Petter */ 0x00011493, /* Petteri */ 0x00032ade, /* Pietari */ 0x0008fc4b, 
    /* Pirkka */ 0x000d2e53, /* Pontus */ 0x0000990e, /* Pyry */ 0x0001ecf8, /* Raafael */ 0x000d26c2, 
    /* Rabbe */ 0x00016a27, /* Rafael */ 0x000bd0b4, /* Ragnar */ 0x0008088b, /* Ragnvald */ 0x000f4415, 
    /* Raimo */ 0x00097017, /* Raine */ 0x0001caca, /* Rainer */ 0x000bbabc, /* Raino */ 0x000423d4, 
    /* Ralf */ 0x0004e491, /* Rami */ 0x0000c841, /* Rasmus */ 0x000c91d9, /* Rauli */ 0x000dbe77, 
    /* Rauni */ 0x000bdcf5, /* Rauno */ 0x000879c0, /* Reijo */ 0x000a7187, /* Reima */ 0x0003ca47, 
    /* Reinhold */ 0x00020cd0, /* Reino */ 0x0006b483, /* Reko */ 0x0000622e, /* Rene */ 0x00027f75, 
    /* Resat */ 0x000a4706, /* Retu */ 0x000895ca, /* Riiko */ 0x0007ff7e, /* Rikard */ 0x00094f0c, 
    /* Rikhard */ 0x0009c799, /* Riku */ 0x00086230, /* Risto */ 0x000cd746, /* Robert */ 0x000f4d9e, 
    /* Robin */ 0x00081d57, /* Roger */ 0x0007ccff, /* Roland */ 0x0009df48, /* Rolf */ 0x000ac99b, 
    /* Ronald */ 0x00067541, /* Roni */ 0x0003b688, /* Ronny */ 0x0000f733, /* Roobert */ 0x0003da98, 
    /* Roope */ 0x0002fe94, /* Roy */ 0x000066a9, /* Ruben */ 0x0009e5a0, /* Rudolf */ 0x000b5620, 
    /* Rufus */ 0x000433f4, /* Runar */ 0x000e858f, /* Rune */ 0x0004dc05, /* Runo */ 0x0001351b, 
    /* Rurik */ 0x0000fd53, /* Sakari */ 0x0004abd5, /* Saku */ 0x000754ed, /* Salomo */ 0x000415cd, 
    /* Salomon */ 0x0009d68f, /* Sam */ 0x000bf56d, /* Sami */ 0x000caf24, /* Sampo */ 0x000c9d67, 
    /* Samppa */ 0x0006d7fe, /* Sampsa */ 0x000b843d, /* Samu */ 0x000df36b, /* Samuel */ 0x00098fbc, 
    /* Samuli */ 0x0001c07a, /* Santeri */ 0x000afd76, /* Santtu */ 0x000cc1b5, /* Saska */ 0x000af580, 
    /* Sasu */ 0x000cccb4, /* Saul */ 0x000dc3f2, /* Sauli */ 0x000d97c7, /* Sebastian */ 0x00056e80, 
    /* Selim */ 0x0002a833, /* Seppo */ 0x00003a13, /* Severi */ 0x000e2a8b, /* Severin */ 0x0005b815, 
    /* Sigfrid */ 0x000780ac, /* Sigurd */ 0x000c28bd, /* Sigvard */ 0x00080180, /* Silvo */ 0x00007839, 
    /* Simo */ 0x000c5ba9, /* Simon */ 0x00058b80, /* Sipi */ 0x00039280, /* Sippo */ 0x000685ab, 
    /* Sisu */ 0x000f9d0c, /* Sixten */ 0x000268cb, /* Soini */ 0x0008d862, /* S?ren */ 0x000f103c, 
    /* Staffan */ 0x000c0cf3, /* Stefan */ 0x000f06a0, /* Sten */ 0x000cd114, /* Stig */ 0x000526bc, 
    /* Sture */ 0x000d439e, /* Styrbj?rn */ 0x000f72f1, /* Sulevi */ 0x0006cff6, /* Sulho */ 0x000eafc1, 
    /* Sulo */ 0x000b30fc, /* Sune */ 0x0008bb60, /* Svante */ 0x0009acb3, /* Sven */ 0x0008057a, 
    /* Sverker */ 0x000ac1e8, /* Sylvester */ 0x0009f069, /* Taani */ 0x000422f9, /* Taavetti */ 0x00020fc2, 
    /* Taavi */ 0x000fbaa0, /* Tage */ 0x0002333c, /* Tahvo */ 0x000d241a, /* Taisto */ 0x0003264b, 
    /* Taito */ 0x00092caf, /* Taneli */ 0x0007fc5c, /* Tapani */ 0x000e16ae, /* Tapio */ 0x0002d84c, 
    /* Tarmo */ 0x000ac926, /* Tarvo */ 0x000c02bc, /* Tatu */ 0x000a62ca, /* Tauno */ 0x00088c60, 
    /* Teemu */ 0x000312fe, /* Teijo */ 0x000a8427, /* Tenho */ 0x0003f020, /* Teodor */ 0x0001cde1, 
    /* Teppo */ 0x0000e603, /* Terho */ 0x000faa34, /* Tero */ 0x000b94ea, /* Teuvo */ 0x0001836e, 
    /* Theo */ 0x0000832f, /* Tihvo */ 0x00090cf5, /* Tiitus */ 0x00061601, /* Timi */ 0x0008c625, 
    /* Timo */ 0x000b6310, /* Tino */ 0x000630d3, /* Tobias */ 0x000783d6, /* Toimi */ 0x000557b1, 
    /* Toivo */ 0x0000391e, /* Tom */ 0x0007ce66, /* Tomas */ 0x000b491b, /* Tomi */ 0x0005ba97, 
    /* Tommi */ 0x000cff6d, /* Tommy */ 0x000bef09, /* Tomppa */ 0x00035927, /* Toni */ 0x0008e954, 
    /* Tony */ 0x000ff930, /* Topi */ 0x0009d68b, /* Topias */ 0x00071cc2, /* Tor */ 0x000fc393, 
    /* Torbj?rn */ 0x00075655, /* Tord */ 0x000ec8b4, /* Tore */ 0x0009f822, /* Torgny */ 0x000d0c92, 
    /* Torkel */ 0x000ec8d6, /* Torolf */ 0x0000325d, /* Torsten */ 0x000338c0, /* Torsti */ 0x0008ed81, 
    /* Torvald */ 0x0007fb3e, /* Touko */ 0x00000f16, /* Tryggve */ 0x000d5d6f, /* Tuisku */ 0x000be0ed, 
    /* Tuomas */ 0x00047cf4, /* Tuomo */ 0x000f39cd, /* Ture */ 0x0008de84, /* Turkka */ 0x000c16c6, 
    /* Turo */ 0x000d379a, /* Tuukka */ 0x000b2e7f, /* Tuure */ 0x0001f8eb, /* Ukko */ 0x0009779d, 
    /* Ulf */ 0x000a2e1a, /* Uljas */ 0x0001d9c0, /* Ulrik */ 0x00013956, /* Uno */ 0x0000f43c, 
    /* Untamo */ 0x000e337b, /* Unto */ 0x0008bbe8, /* Uolevi */ 0x000fa3c8, /* Uoti */ 0x000974ea, 
    /* Urban */ 0x0002c40d, /* Urho */ 0x0003bca1, /* Urmas */ 0x0008efe9, /* Urpo */ 0x000824f8, 
    /* Usko */ 0x000c8555, /* Uuno */ 0x00060da2, /* V?in?m? */ 0x0005e4b3, /* V?in? */ 0x0005e4b3, 
    /* Valdemar */ 0x000e6884, /* Valentin */ 0x0000888d, /* Valfrid */ 0x000c1c77, /* Valio */ 0x000ed138, 
    /* Valo */ 0x000acb62, /* Valter */ 0x00052e3e, /* Valto */ 0x0002bd24, /* Valtteri */ 0x0002163a, 
    /* Veeti */ 0x0002b4c9, /* Veijo */ 0x000ad747, /* Veikko */ 0x0004ddd6, /* Veini */ 0x0005b776, 
    /* Veli */ 0x0000c68b, /* Verner */ 0x00028ef4, /* Verneri */ 0x00075dbf, /* Vertti */ 0x000f42ae, 
    /* Vesa */ 0x00014027, /* Vidar */ 0x00064ebe, /* Vihtori */ 0x000620ca, /* Viking */ 0x00055045, 
    /* Viktor */ 0x000db5cc, /* Vilhelm */ 0x0002103d, /* Vilho */ 0x0001c896, /* Vili */ 0x000a3fef, 
    /* Viljami */ 0x00048532, /* Viljo */ 0x0007aa14, /* Ville */ 0x0008e48c, /* Vilppu */ 0x000dbe94, 
    /* Vinski */ 0x00055f93, /* Visa */ 0x000bb943, /* Voitto */ 0x00042fa4, /* Volmar */ 0x000ecda4, 
    /* Volter */ 0x0005905f, /* Waldemar */ 0x0004681a, /* Waltteri */ 0x000816a4, /* Werneri */ 0x0000560b, 
    /* Wiljami */ 0x00038e86, /* William */ 0x00030bf7, /* Ylermi */ 0x000a2886, /* Yngvar */ 0x000a9d51, 
    /* Yngve */ 0x00075ede, /* Yrj?n? */ 0x0007a48a, /* Yrj? */ 0x0007a48a, /* Zacharias */ 0x00097bba
};

#define HASH_MASK  0x0fffff

static unsigned int crc_table[256];
static gboolean table_generated = FALSE;

static void gen_table(void)                /* build the crc table */
{
    unsigned int crc, poly;
    int i, j;

    if (table_generated)
        return;

    poly = 0xEDB88320L;
    for (i = 0; i < 256; i++) {
        crc = i;
        for (j = 8; j > 0; j--) {
            if (crc & 1)
                crc = (crc >> 1) ^ poly;
            else
                crc >>= 1;
        }
        crc_table[i] = crc;
    }

    table_generated = TRUE;
}


static unsigned int hash(gchar *name)
{
    unsigned int crc = 0xFFFFFFFF;
    gchar       *uppercase = g_utf8_strup(name, -1);
    gchar       *ch = uppercase;

    while (*ch > 0) {
        crc = (crc>>8) ^ crc_table[(crc^(*ch)) & 0xFF];
        ch++;
    }

    g_free(uppercase);

    return (crc & HASH_MASK);
}

static char buf[100];

gint find_gender(const gchar *name)
{
    unsigned int h;
    gint i, res = 0;

    for (i = 0; name[i] && i < (sizeof(buf)-1); i++) {
        buf[i] = name[i];
        if (name[i] == '-' || name[i] <= ' ')
            break;
    }

    buf[i] = 0;

    gen_table();
    h = hash(buf);

    for (i = 0; i < NUM_MEN; i++) {
        if (h == men_names[i]) {
            res |= IS_MALE;
            break;
        }
    }
                
    for (i = 0; i < NUM_WOMEN; i++) {
        if (h == women_names[i]) {
            res |= IS_FEMALE;
            break;
        }
    }

    //g_print("name=%s hash=0x08%d res=%d\n", name, h, res);

    return res;
}

#ifdef NAME_PROGRAM

int main(int argc, char *argv[])
{
    FILE *f;
    int i, j, tot = 0;

    gen_table();

    if (argc < 2)
        return 1;

    f = fopen(argv[1], "r");
    if (!f)
        return 1;

    i = 0;
    while (!feof(f)) {
        if (fscanf(f, "%s", buf)) {
            for (j = 0; buf[j]; j++) {
                if (buf[j] == '\n') {
                    buf[j] = 0;
                    break;
                }
            }

            g_print("/* %s */ 0x%08x, ", buf, hash(buf));
            i++;
            if (i >= 4) {
                i = 0;
                g_print("\n");
            }
            tot++;
        }
    }

    fclose(f);

    g_print("\ntotal %d names\n", tot);

    return 0;
}

#endif
