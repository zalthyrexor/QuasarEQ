#include <array>

namespace zlth::dsp::window::coefficients
{
    using Real = double;
    inline constexpr std::array<Real, 3> blackman_exact
    {
        7938.0 / 18608.0, -9240.0 / 18608.0,
        1430.0 / 18608.0
    };
    inline constexpr std::array<Real, 3> blackman_approx
    {
        0.42, -0.50,
        0.08
    };
    inline constexpr std::array<Real, 3> blackman_harris_67
    {
        0.42323, -0.49755,
        0.07922
    };
    inline constexpr std::array<Real, 3> blackman_harris_61
    {
        0.44959, -0.49364,
        0.05677
    };
    inline constexpr std::array<Real, 4> blackman_harris_92
    {
        0.35875, -0.48829,
        0.14128, -0.01168
    };
    inline constexpr std::array<Real, 4> blackman_harris_74
    {
        0.40217, -0.49703,
        0.09392, -0.00183
    };
    inline constexpr std::array<Real, 2> hann
    {
        0.5, -0.5
    };
    inline constexpr std::array<Real, 2> hamming
    {
        0.54, -0.46
    };
    inline constexpr std::array<Real, 3> nuttall3
    {
        0.375, -0.500,
        0.125
    };
    inline constexpr std::array<Real, 3> nuttall3a
    {
        0.40897, -0.50000,
        0.09103
    };
    inline constexpr std::array<Real, 3> nuttall3b
    {
        0.4243801, -0.4973406,
        0.0782793
    };
    inline constexpr std::array<Real, 4> nuttall4
    {
        0.31250, -0.46875,
        0.18750, -0.03125
    };
    inline constexpr std::array<Real, 4> nuttall4a
    {
        0.338946, -0.481973,
        0.161054, -0.018027
    };
    inline constexpr std::array<Real, 4> nuttall4b
    {
        0.355768, -0.487396,
        0.144232, -0.012604
    };
    inline constexpr std::array<Real, 4> nuttall4c
    {
        0.3635819, -0.4891775,
        0.1365995, -0.0106411
    };
    inline constexpr std::array<Real, 3> sft3f
    {
        0.26526, -0.50000,
        0.23474
    };
    inline constexpr std::array<Real, 4> sft4f
    {
        0.21706, -0.42103,
        0.28294, -0.07897
    };
    inline constexpr std::array<Real, 5> sft5f
    {
        0.18810, -0.36923,
        0.28702, -0.13077,
        0.02488
    };
    inline constexpr std::array<Real, 3> sft3m
    {
        0.28235, -0.52105,
        0.19659
    };
    inline constexpr std::array<Real, 4> sft4m
    {
        0.241906, -0.460841,
        0.255381, -0.041872
    };
    inline constexpr std::array<Real, 5> sft5m
    {
        0.2096710, -0.4073310,
        0.2812250, -0.0926690,
        0.0091036
    };
    inline constexpr std::array<Real, 7> blackman_harris_7
    {
        0.27105140069342, -0.43329793923448,
        0.21812299954311, -0.06592544638803,
        0.01081174209837, -0.00077658482522,
        0.00001388721735
    };
    inline constexpr std::array<Real, 2> albrecht_2term
    {
        5.383553946707251e-001, -4.616446053292749e-001
    };
    inline constexpr std::array<Real, 3> albrecht_3term
    {
        4.243800934609435e-001, -4.973406350967378e-001,
        7.827927144231873e-002
    };
    inline constexpr std::array<Real, 4> albrecht_4term
    {
        3.635819267707608e-001, -4.891774371450171e-001,
        1.365995139786921e-001, -1.064112210553003e-002,
    };
    inline constexpr std::array<Real, 5> albrecht_5term
    {
        3.232153788877343e-001, -4.714921439576260e-001,
        1.755341299601972e-001, -2.849699010614994e-002,
        1.261357088292677e-003
    };
    inline constexpr std::array<Real, 6> albrecht_6term
    {
        2.935578950102797e-001, -4.519357723474506e-001,
        2.014164714263962e-001, -4.792610922105837e-002,
        5.026196426859393e-003, -1.375555679558877e-004
    };
    inline constexpr std::array<Real, 7> albrecht_7term
    {
        2.712203605850388e-001, -4.334446123274422e-001,
        2.180041228929303e-001, -6.578534329560609e-002,
        1.076186730534183e-002, -7.700127105808265e-004,
        1.368088305992921e-005
    };
    inline constexpr std::array<Real, 8> albrecht_8term
    {
        2.533176817029088e-001, -4.163269305810218e-001,
        2.288396213719708e-001, -8.157508425925879e-002,
        1.773592450349622e-002, -2.096702749032688e-003,
        1.067741302205525e-004, -1.280702090361482e-006
    };
    inline constexpr std::array<Real, 9> albrecht_9term
    {
        2.384331152777942e-001, -4.005545348643820e-001,
        2.358242530472107e-001, -9.527918858383112e-002,
        2.537395516617152e-002, -4.152432907505835e-003,
        3.685604163298180e-004, -1.384355593917030e-005,
        1.161808358932861e-007
    };
    inline constexpr std::array<Real, 10> albrecht_10term
    {
        2.257345387130214e-001, -3.860122949150963e-001,
        2.401294214106057e-001, -1.070542338664613e-001,
        3.325916184016952e-002, -6.873374952321475e-003,
        8.751673238035159e-004, -6.008598932721187e-005,
        1.710716472110202e-006, -1.027272130265191e-008
    };
    inline constexpr std::array<Real, 11> albrecht_11term
    {
        2.151527506679809e-001, -3.731348357785249e-001,
        2.424243358446660e-001, -1.166907592689211e-001,
        4.077422105878731e-002, -1.000904500852923e-002,
        1.639806917362033e-003, -1.651660820997142e-004,
        8.884663168541479e-006, -1.938617116029048e-007,
        8.482485599330470e-010
    };
}

// https://www.researchgate.net/publication/2995027_On_the_Use_of_Windows_for_Harmonic_Analysis_With_the_Discrete_Fourier_Transform
// p184
// blackman_exact, blackman_approx
// p186
// bh67, bh61, bh92, bh74
// https://hdl.handle.net/11858/00-001M-0000-0013-557A-5
// p31
// hann
// p32
// hamming
// p33
// nuttall3, nuttall3a, nuttall3b, nuttall4, nuttall4a, nuttall4b, nuttall4c
// p40
// sft3f, sft4f, sft5f, sft3m, sft4m, sft5m
// https://linearaudio.net/sites/linearaudio.net/files/Virtins%20Sound%20Card%20Instrument%20Manual%20%26%20FFT%20windows.pdf
// p77
// blackman_harris_7
// chrome-extension://efaidnbmnnnibpcajpcglclefindmkaj/http://dihana.cps.unizar.es/proceedings/ICASSP/2001/MAIN/papers/pap45.pdf
// albrecht_2term, albrecht_3term, albrecht_4term...
