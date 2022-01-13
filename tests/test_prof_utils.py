#! /usr/bin/env python
"""
Tests the prof_utils.py script
"""
import os
import numpy as np
from numpy.testing import assert_almost_equal
from vcstools.metadb_utils import get_common_obs_metadata
import psrqpy

from vcstools.prof_utils import sn_calc
from vcstools.general_utils import setup_logger

import logging
logger = logging.getLogger(__name__)
#logger = setup_logger(logger, log_level="DEBUG")

"""
#Global obsid information to speed things up
md_dict={}
obsid_list = [1222697776, 1226062160, 1225713560, 1117643248]
for obs in obsid_list:
    md_dict[str(obs)] = get_common_obs_metadata(obs, return_all=True)

query = psrqpy.QueryATNF(loadfromdb=data_load.ATNF_LOC).pandas
"""
# The three tests of the flux calculation methods. They should be close to imaging results:
test_pulsars = []
# J1820-0427 298.5 sigma. A bright pulsar with two components and a scattering tail that is about 50% of the profile
J1820_0427_profile = np.array([0.00974835457, 0.00275491384, 0.00109578621, -0.00279699934, 0.000562140998, 0.00113477532, 0.00339133085, -0.0109063454, -0.00964722073, 0.00111819766, 0.00510179576, -0.00238241189, -0.0159297665, 0.00526543335, 0.00104425447, -0.00416604215, -0.00577553402, -0.0107780994, -0.0082326258, -0.0134817171, -0.00582210704, -0.0124964885, -0.00268844238, -0.00974819377, -0.016102884, -0.0141605262, -0.00882266424, -0.0111797553, -0.0167948192, -0.00793797119, -0.00794229791, -0.000902770299, -2.88211273e-05, -0.00844239888, -0.00128177167, -0.00585817926, 0.00461725154, 0.118554769, 0.631823206, 0.978832608, 1.0, 0.833592824, 0.657390729, 0.523552684, 0.423043522, 0.361585454, 0.317663881, 0.288038185, 0.253027957, 0.223348542, 0.190868669, 0.16998052, 0.150608232, 0.158389622, 0.168857566, 0.17383913, 0.194769962, 0.185245049, 0.18829777, 0.171631238, 0.13852924, 0.136540455, 0.128550629, 0.121552035, 0.126898161, 0.11610917, 0.0966390774, 0.0836497683, 0.0709359634, 0.0663007999, 0.062169121, 0.0804554427, 0.0738022707, 0.0652449194, 0.0631586179, 0.0528215401, 0.0435259772, 0.037796751, 0.0423237659, 0.0357590644, 0.0347282449, 0.0329981882, 0.0295279872, 0.0228235617, 0.0293093176, 0.0313360707, 0.0207441587, 0.02619471, 0.00945243598, 0.0276467383, 0.0149054666, 0.011583468, 0.00358243583, 0.010961684, 0.0240656226, -0.00276778182, 0.00860143302, 0.00782033821, 0.011622846, 0.00154440406, 0.00617944115, -0.000217641207, 0.0141621455, 0.0112963973, 0.0187439007, 0.00671303776, 0.014185432, 0.00511642883, 0.00926998444, 0.00543009184, 0.00484238691, 0.00771907348, 0.0186740412, -0.00596104829, -0.00456473254, -1.99246096e-05, 0.00802933345, 0.00264869039, 0.0164105337, -0.00607378612, 0.00422687429, 0.0106680503, -0.00803578427, -0.00855158784, -0.0111729006, -0.00683086519, -0.00433629136, -0.00145620176])
J1820_0427_profile_bool = [True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True]
J1820_0427_data = [0.00808072162938938, 11.269025292925557, 379.3714299196362, 47.10349059977232, 123.75132393659315, 10.69048613727136]
test_pulsars.append( ("J1820-0427", 298.5, J1820_0427_profile, J1820_0427_profile_bool, J1820_0427_data) )
# J1825-0935 263.0 sigma. A bright pulsar with a small interpulse
J1825_0935_profile = np.array([0.00314088125, 0.000556045128, 0.00105395013, 0.0045659471, 0.000470929106, -0.000409939544, -0.00412380941, 0.00288133825, 0.0103584648, -0.00706736449, -0.000329613318, 0.00548152716, 0.00668359412, -0.00481527652, 0.00774982755, -0.00279912297, -0.00403820169, -0.00168432796, 0.000365357593, 0.0031319756, 0.000397067214, 0.0107867029, 0.0191924172, 0.0207955352, 0.018085596, 0.0247475429, 0.0163618917, 0.0198945505, 0.0172405494, 0.0250506153, 0.0946288765, 0.499182263, 1.0, 0.593519933, 0.135216572, 0.0189654022, 0.00493651504, -0.000583671957, 0.00145633429, -0.00247092049, 0.00340683702, 0.00189445108, -0.00245343584, 0.000542236211, 0.00387792879, 0.0181227866, 0.00592787911, -0.00376045883, 0.0049415421, 0.00555503833, 0.00204712627, 0.00205753797, 0.0036205589, -0.000353569177, 0.00817348899, 0.01150161, 0.00514606606, 0.00020703299, 0.00484254839, -0.00216935931, 0.00545115511, 0.0109659712, -0.000176346712, 0.0054985373, 0.00441087186, 0.00814568892, 0.00871168261, 0.00776448921, 0.0023226238, -0.000963073342, -0.00339329842, 0.00568027449, 0.00280664424, -0.00246610319, 0.00131316992, 0.000193402873, -0.00158659617, -0.00183816535, -0.00666429952, 0.0033070112, 0.00128840946, -0.00409108888, 0.0073903315, 0.00100847286, 0.00455245796, 0.00993097978, -0.000629802544, -0.0088467171, 0.00566167921, 0.00443358643, 0.00194643739, 0.00724301346, 0.0081186625, 0.0155670915, 0.0196579249, 0.0117332429, 0.0249344743, 0.0400196965, 0.0639603915, 0.0317363231, 0.00753277035, 0.00222084149, 0.0037240295, 0.00248847523, -0.00204282341, 0.00200497056, 0.00673926304, 0.0134845754, -0.00201286054, 0.00573517662, 0.00362153543, 0.00730374381, 0.00889853625, 0.00706360068, 0.00481938682, 0.00830079167, -0.00203974254, 0.00745314557, 0.00618885252, -0.00471675388, 0.00923441341, 0.000585897973, 0.0116158121, 0.00490610861, -0.00228758785, 0.00194633768, 0.00473734135, 0.00743085737])
J1825_0935_profile_bool = [True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True]
J1825_0935_data = [0.0031191764012526266, 3.0603445862819347, 555.6058499206861, 192.24964804679556, 320.59744989042974, 36.300512971625636]
test_pulsars.append( ("J1825-0935", 263.0, J1825_0935_profile, J1825_0935_profile_bool, J1825_0935_data) )
# J1834-0010 16.1 sigma
J1834_0010_profile = np.array([-0.2517820717787388, 0.1073720480401459, -0.03562029417799154, 0.12123301331614068, 0.022520123232647064, -0.03921288035870826, -0.00942285788080348, -0.08900187838661028, -0.0597638186089579, 0.07171540061051168, -0.11003798832690412, -0.07754092698059321, -0.005151024442136903, -0.06281649427867554, -0.053737611989087365, 0.06781081865710001, -0.029738541531959966, 0.08740268176424729, -0.04827681586605577, -0.12847620106115867, -0.05090738062152244, -0.01104385579823273, -0.13709881451627448, 0.03722849783537372, -0.01159681782225459, 0.18298493341726202, 0.07629630076944585, -0.009082114320017978, 0.16891643616191482, -0.20750281089398878, 0.01447809793486095, -0.17549843339571278, -0.011874560911267427, 0.0837931415388911, -0.3776954883456491, -0.0685699285838, -0.023140790634872684, 0.21150727061938127, 0.04155030435439834, -0.01871578498396509, -0.03225650145130725, -0.14180002981110043, 0.15766137991183468, -0.3079289054653312, -0.2103799157152547, -0.26779795815540575, -0.16311319365160062, 0.04472990275769002, 0.2057219613319569, -0.3741992335303586, -0.1743795353683869, -0.13591797564496597, -0.2620446961474828, -0.25851303425725924, 0.1743837709637479, -0.010241846784127956, 0.06913220035577065, 0.2274924019455588, 0.38604218214629044, 0.5534242614965105, 0.39911902285770867, 0.6414705436855919, 0.7880298170311245, 0.9850721704515689, 0.907415237223024, 1.0, 0.5084029165643795, 0.46409488204695876, 0.42737076730468254, 0.5010080929680137, 0.09016464898052999, 0.17994845368449675, 0.08770692416286939, -0.029463512123872635, 0.3539655259719344, 0.06465133595209765, -0.3172996333698848, -0.1636517085552666, 0.0433149895231194, 0.08655943341493454, -0.22292771786909613, -0.2628809406155497, -0.219690339599241, 0.05958428200907011, 0.14130675723678124, -0.027028539151686015, -0.0395821477212221, 0.040630090845121654, -0.07026903743758411, -0.06573744873775388, 0.08410298359656589, -0.3382789713822445, 0.135255076475651, 0.020477417576418622, 0.25000363437752965, -0.11972255558421015, -0.048019343542813675, -0.20919632008872022, 0.03227351460714043, -0.430842283157619, 0.08651209475878921, -0.24400147474794415, 0.25250771575728637, -0.1136311603181063, -0.008050579588773909, -0.036413912318377414, 0.07707627297146324, -0.1493332942089684, -0.0786053790922069, 0.056969017245107494, 0.03837770552492264, -0.15263450859202404, 0.0032237911776078726, 0.22013401920733605, 0.056147779863301814, -0.014655162069905482, -0.03770183390049333, 0.30842117288428333, -0.2566691957541137, -0.06079811871028795, -0.21165482885864825, -0.10278832512290409, 0.021476525974317578, 0.044770108309688125, 0.018076697287914195, -0.11719866002626701, -0.050156578335739326, -0.04280315775692326])
J1834_0010_profile_bool = [True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True]
J1834_0010_data  = [0.07982397127335845, 13.729849955263283, 13.20398964616978, 3.655260401205086, 12.527565141747761, 1.6740663089611518]
test_pulsars.append( ("J1834-0010", 16.1, J1834_0010_profile, J1834_0010_profile_bool, J1834_0010_data) )
# J1834-0426 122.2 sigma
J1834_0426_profile = np.array([0.03188963756604254, -0.04120575861389495, 0.00032700654815682193, -0.0321321953447013, -0.025940308360317587, -0.012129328111142904, -0.014598350824467728, 0.036941180661511135, -0.03354978481558185, 0.006244324652222429, 0.01823964574378, -0.019987448073553522, 0.005360364629341021, -0.018127747515636134, 0.005262596032057901, -0.003743801342191608, -0.005477752112409534, 0.007138439584446249, -0.029843587841378454, -0.025265476001706613, -0.013253280651825971, -0.035524804303232835, -0.012224985701456742, -0.024355563010791118, 0.00025615673908981247, 0.006626403116230913, -0.018895340004014882, -0.008219080927340291, 0.02822630921048982, -0.035955974027350464, 0.01910906327420637, 0.02462614549484144, 0.04486425892987841, -0.025187092795219822, 0.0011296997014365383, 0.013220595598720922, -0.024855706093336392, -0.008769653620818364, 0.030230165329289793, -0.012401854971647586, 0.025751974486163863, 0.04211278900303641, 0.09215353037620266, 0.11456689437954147, 0.12790408463970898, 0.13622258348802785, 0.09544697305767337, 0.06865425886889376, 0.057003528620851, 0.037340009206962135, 0.08352980751498824, 0.09448959138455454, 0.05896330194705719, 0.10374668899382544, 0.10910428027794318, 0.18331787960899443, 0.3126019068979614, 0.5263210803137282, 0.7849299200982057, 0.9641155030341644, 1.0, 0.9673668676897906, 0.8690644477946352, 0.7112240427786424, 0.6220749766971471, 0.4927037772380369, 0.41818191983897546, 0.37665192796058916, 0.3167510703999343, 0.3411021566749806, 0.24051235724787667, 0.26636009227441776, 0.2250049000515466, 0.22570960384864444, 0.22218531220700954, 0.14374657105975183, 0.19648750006758084, 0.14261054576679347, 0.15946565325539447, 0.20565283005149795, 0.173950251689237, 0.17695122056395804, 0.14178305862969812, 0.12167953913574169, 0.10335101390645847, 0.04038817909372176, 0.04263074456922442, 0.05239626280911321, 0.047298015409128216, 0.05244774102481507, 0.07283853683319878, 0.009095356842399883, 0.023069284753713105, 0.0018909315740451038, 0.055579564736769095, 0.018599330976996717, -0.005445714477228695, 0.03761248087774213, 0.01276449391647592, 0.018054898140390078, 0.03156085857865058, 0.028394969009133467, 0.05481395907847909, 0.018033043009414006, 0.018218301117757282, 0.002061316051585213, 0.00280967492091052, -0.03525244301190222, -0.023531690800662745, 0.03517767579372328, 0.024066797635138134, 0.026475622156260554, -0.015106317050487393, -0.003184784620836418, 0.021968567087123286, -0.0020480694563194675, 0.03326609689460006, -0.012172776221902788, -0.014908061761980318, -0.04037915451239457, -0.0024375433434360174, 0.0015382968281571578, -0.0029163555973897434, -0.0023209826448969597, -0.012142670227085999, -0.005229591515354183, 0.0040233659539347455, -0.002122603179509336])
J1834_0426_profile_bool = [True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True]
J1834_0426_data = [0.022160061202548608, 15.090857513599753, 154.3214005278128, 17.64979491657411, 45.12622915883422, 4.119442274240146]
test_pulsars.append( ("J1834-0426", 122.2, J1834_0426_profile, J1834_0426_profile_bool, J1834_0426_data) )
# J1849-0636 23.1 sigma
J1849_0636_profile = np.array([0.06379222330341695, 0.1554693500967726, -0.07765938556551845, 0.03256034697098465, 0.13798479084720772, 0.10414650294606423, 0.26743614652877323, 0.04124454357170729, 0.19321753792106522, 0.18263957893570776, 0.029407682816505572, -0.1083322936584711, -0.03947055944181317, -0.05355704985631753, 0.10837709760466695, 0.21418561073700054, 0.17007528611948308, -0.16472116135269715, -0.07210316541733164, -0.18682148802750786, 0.16878737629985563, 0.04039854361686357, -0.002617843644963124, 0.1311908393771985, -0.00525437123562307, 0.10640304813633292, 0.07215592594216581, -0.20465969117135469, -0.03596141082011776, 0.04778775127378707, 0.11111401193515624, 0.03013061606437323, 0.14978484313113866, 0.09048535486624441, -0.03745910661782424, 0.1142166314408167, -0.016230285565828683, 0.1487071951970975, 0.05517502555646196, -0.13616760323891297, -0.08696754152469177, 0.04799982755674516, -0.043903117348842974, 0.1534525879296549, -0.10481823724060622, 0.021192403831608243, -0.2722713502152186, 0.11961446362171708, 0.043261350355153785, -0.11173005017922993, -0.028625370955029854, 0.14977084103994617, 0.39462639128242144, 0.5152114636893234, 0.4626107453929705, 0.754031071580159, 0.8896896219554584, 0.7118585278172833, 1.0, 0.741543704750863, 0.8710450530288634, 0.86800006294792, 0.8557734041999882, 0.7719499559186095, 0.8947636139497238, 0.6849759255780965, 0.9013122499718094, 0.45397272669256933, 0.6221724195693203, 0.6890949794146664, 0.5727213166449776, 0.18512595027174733, 0.5747781294032598, 0.538408087923619, 0.5903565154927249, 0.3636271891047493, 0.3942396420721602, 0.49602703718409036, 0.46776779811937635, 0.4932037158967997, -0.014011515533955063, 0.1848162534613462, 0.19255740959205672, 0.1900760575930087, 0.4184711420532749, 0.16131893747901493, 0.4618517472793123, 0.3160808316179842, 0.3275995035882008, 0.04189772662553051, 0.15752077915137802, 0.3511973029790739, 0.17609930103921997, 0.130256722172949, 0.10217922028760025, 0.19034607561498526, 0.29957992856976096, 0.04384158571136766, 0.2326695341100711, 0.36958569981773376, 0.16523460778069057, 0.046530240046190885, 0.09407927451268465, 0.07581491106812509, 0.19279335561092983, 0.08139656252373535, 0.22998382445294957, 0.15739183796110828, 0.21137479317585978, 0.2845970068767445, -0.1332663520972955, -0.06638788805692628, 0.1763939621468111, 0.1178971811257023, 0.3599629987404284, 0.20734949609265751, 0.11683471761556723, 0.07067102015871898, -0.012393801811741424, 0.17629256410352856, 0.2090549530307204, 0.20237317296019114, 0.17767514233684262, -0.0868470774368965, 0.05137245021227263, 0.287937074484348, 0.20905696448354438, -0.11275392056494173])
J1849_0636_profile_bool = [True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True, True]
J1849_0636_data = [0.09557737284098483, 21.981503499225628, 60.91390123372835, 7.977734727827856, 10.46272742465659, 1.21626735793653]
test_pulsars.append( ("J1849-0636", 23.1, J1849_0636_profile, J1849_0636_profile_bool, J1849_0636_data) )
#---------------------------------------------------------------------------------------------------------------------

def test_sn_calc():
    for sn_method in ["eqn_7.1", "simple"]:
        for pulsar, sigma, profile, profile_bool, data in test_pulsars:
            noise_std, w_equiv_bins, exp_e_sn, exp_e_u_sn, exp_s_sn, exp_s_u_sn = data
            sn, u_sn = sn_calc(profile, profile_bool, noise_std, w_equiv_bins, sn_method=sn_method)
            print(f"{pulsar} {sn_method:7s} {sigma:7.3f} {sn:6.1f} {u_sn:6.1f}")
            #print(f"{pulsar} {sn_method:7s} {sigma:7.3f} {sn} {u_sn}")
            if sn_method == "eqn_7.1":
                assert_almost_equal(sn,   exp_e_sn,   decimal=2)
                assert_almost_equal(u_sn, exp_e_u_sn, decimal=2)
            else:
                assert_almost_equal(sn,   exp_s_sn,   decimal=2)
                assert_almost_equal(u_sn, exp_s_u_sn, decimal=2)

