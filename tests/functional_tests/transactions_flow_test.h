// Copyright (c) 2012-2014, The CryptoNote developers, The Bytecoin developers
//
// This file is part of Bytecoin.
//
// Bytecoin is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Bytecoin is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Bytecoin.  If not, see <http://www.gnu.org/licenses/>.


bool transactions_flow_test(std::string& working_folder, 
                            std::string path_source_wallet, 
                            std::string path_terget_wallet, 
                            std::string& daemon_addr_a, 
                            std::string& daemon_addr_b, 
                            uint64_t amount_to_transfer, size_t mix_in_factor, size_t transactions_count, size_t transactions_per_second);
