// Copyright (c) 2011-2021 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0

#ifndef __CONV_CONF_INFO_HPP__
#define __CONV_CONF_INFO_HPP__

#include <systemc.h>

//
// Configuration parameters for the accelerator.
//
class conf_info_t
{
public:

    //
    // constructors
    //
    conf_info_t()
    {
        /* <<--ctor-->> */
        this->mem_output_addr = 10000;
        this->mem_input_addr = 10000;
        this->S = 5;
        this->R = 5;
        this->Q = 28;
        this->P = 28;
        this->M = 6;
        this->C = 3;
    }

    conf_info_t(
        /* <<--ctor-args-->> */
        int32_t mem_output_addr, 
        int32_t mem_input_addr, 
        int32_t S, 
        int32_t R, 
        int32_t Q, 
        int32_t P, 
        int32_t M, 
        int32_t C
        )
    {
        /* <<--ctor-custom-->> */
        this->mem_output_addr = mem_output_addr;
        this->mem_input_addr = mem_input_addr;
        this->S = S;
        this->R = R;
        this->Q = Q;
        this->P = P;
        this->M = M;
        this->C = C;
    }

    // equals operator
    inline bool operator==(const conf_info_t &rhs) const
    {
        /* <<--eq-->> */
        if (mem_output_addr != rhs.mem_output_addr) return false;
        if (mem_input_addr != rhs.mem_input_addr) return false;
        if (S != rhs.S) return false;
        if (R != rhs.R) return false;
        if (Q != rhs.Q) return false;
        if (P != rhs.P) return false;
        if (M != rhs.M) return false;
        if (C != rhs.C) return false;
        return true;
    }

    // assignment operator
    inline conf_info_t& operator=(const conf_info_t& other)
    {
        /* <<--assign-->> */
        mem_output_addr = other.mem_output_addr;
        mem_input_addr = other.mem_input_addr;
        S = other.S;
        R = other.R;
        Q = other.Q;
        P = other.P;
        M = other.M;
        C = other.C;
        return *this;
    }

    // VCD dumping function
    friend void sc_trace(sc_trace_file *tf, const conf_info_t &v, const std::string &NAME)
    {}

    // redirection operator
    friend ostream& operator << (ostream& os, conf_info_t const &conf_info)
    {
        os << "{";
        /* <<--print-->> */
        os << "mem_output_addr = " << conf_info.mem_output_addr << ", ";
        os << "mem_input_addr = " << conf_info.mem_input_addr << ", ";
        os << "S = " << conf_info.S << ", ";
        os << "R = " << conf_info.R << ", ";
        os << "Q = " << conf_info.Q << ", ";
        os << "P = " << conf_info.P << ", ";
        os << "M = " << conf_info.M << ", ";
        os << "C = " << conf_info.C << "";
        os << "}";
        return os;
    }

        /* <<--params-->> */
        int32_t mem_output_addr;
        int32_t mem_input_addr;
        int32_t S;
        int32_t R;
        int32_t Q;
        int32_t P;
        int32_t M;
        int32_t C;
};

#endif // __CONV_CONF_INFO_HPP__
