//  Created by Alberto Santini on 18/09/13.
//  Copyright (c) 2013 Alberto Santini. All rights reserved.
//

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

#include <preprocessing/graph_generator.h>

namespace GraphGenerator {
    std::shared_ptr<Graph> create_graph(const ProblemData& data, const ProgramParams& params, std::shared_ptr<VesselClass> vessel_class) {
        auto g = std::make_shared<Graph>(BGraph(), vessel_class, "Graph for " + vessel_class->name);
    
        /*  Add vertices */
        Vertex v_h1, v_h2;
        Node n_h1, n_h2;
        for(auto p : data.ports) {
            /*  Create nodes H1 and H2 */
            if(p->hub) {
                v_h1 = add_vertex(g->graph);
                g->graph[v_h1] = std::make_shared<Node>(p, PickupType::PICKUP, NodeType::H1, 0, vessel_class);
                n_h1 = *g->graph[v_h1];
                v_h2 = add_vertex(g->graph);
                g->graph[v_h2] = std::make_shared<Node>(p, PickupType::DELIVERY, NodeType::H2, data.num_times - 1, vessel_class);
                n_h2 = *g->graph[v_h2];
                continue;
            }
            for(auto t = 0; t < data.num_times; t++) {
                for(auto pu : {PickupType::PICKUP, PickupType::DELIVERY}) {
                    auto v = add_vertex(g->graph);
                    g->graph[v] = std::make_shared<Node>(p, pu, NodeType::REGULAR_PORT, t, vessel_class);
                }
            }
        }
    
        /*  Add hub-to-port edges */
        for(auto p : data.ports) {
            if(p->hub) {
                continue;
            }
        
            if(!p->allowed[vessel_class]) {
                continue;
            }
        
            for(const auto& sc : vessel_class->bunker_cost) {
                auto distance = data.distances.at(std::make_pair(n_h1.port, p));
                auto speed = sc.first, u_cost = sc.second;
                auto arrival_time = 0 + (int) ceil(distance / speed);
                        
                if(arrival_time >= data.num_times) {
                    continue;
                }
            
                auto final_time_pu = final_time(data, *p, arrival_time, PickupType::PICKUP);
                        
                if(final_time_pu <= latest_departure(data, p, n_h2.port, *vessel_class)) {
                    if(final_time_pu >= data.num_times - 1 - p->pickup_transit) {
                        // Normal arc: arrives at a time when it's allowed
                        auto cost_pu = distance * u_cost + vessel_class->base_cost * (final_time_pu - 0) + p->fee[vessel_class];
                        create_edge(*n_h1.port, PickupType::PICKUP, 0, *p, PickupType::PICKUP, final_time_pu, g, cost_pu);
                    } else {
                        // Travel + wait arc: arrives at a time when it's not allowed
                        auto overall_final_time = data.num_times - 1 - p->pickup_transit;
                        auto cost_pu = distance * u_cost + vessel_class->base_cost * (p->pickup_transit - 0) + p->fee[vessel_class];
                        create_edge(*n_h1.port, PickupType::PICKUP, 0, *p, PickupType::PICKUP, overall_final_time, g, cost_pu);
                    }
                }
            
                auto final_time_de = final_time(data, *p, arrival_time, PickupType::DELIVERY);
                        
                if( (final_time_de <= latest_departure(data, p, n_h2.port, *vessel_class)) &&
                    (final_time_de <= p->delivery_transit)) {
                
                    auto cost_de = distance * u_cost + vessel_class->base_cost * (final_time_de - 0) + p->fee[vessel_class];
                
                    create_edge(*n_h1.port, PickupType::PICKUP, 0, *p, PickupType::DELIVERY, final_time_de, g, cost_de);
                }
            }
        }
        
        /*  Add port-to-hub edges */
        for(auto p : data.ports) {
            if(p->hub || !p->allowed[vessel_class]) {
                continue;
            }
            
            auto pickup_departure_time = std::max(std::min(earliest_arrival(data, p, n_h1.port, *vessel_class), data.num_times - 1 - p->pickup_transit), 1);
            auto delivery_departure_time = std::max(std::min(earliest_arrival(data, p, n_h1.port, *vessel_class), p->delivery_transit), 1);
            
            assert(pickup_departure_time >= 1);
            assert(delivery_departure_time >= 1);
            
            // For pickup nodes:
            for(auto departure_time = pickup_departure_time; departure_time < latest_departure(data, p, n_h2.port, *vessel_class); ++departure_time) {
                auto falls_in_tw = false;
                for(const auto& tw : p->closing_time_windows) {
                    if(departure_time > tw.first && departure_time <= tw.second) {
                        falls_in_tw = true;
                        break;
                    }
                }
            
                if(falls_in_tw) {
                    continue;
                }
                
                for(const auto& sc : vessel_class->bunker_cost) {
                    auto distance = data.distances.at(std::make_pair(p, n_h2.port));
                    auto speed = sc.first, u_cost = sc.second;
                    auto arrival_time = departure_time + (int) ceil(distance / speed);
                    
                    if(arrival_time >= data.num_times) {
                        continue;
                    }
                    
                    // Thew following works the same for:
                    // 1) When arrival_time == data.num_times - 1 => arrival at exact time => no waiting at hub
                    // 2) When arrival_time < data.num_times - 1 => early arrival => waiting at the hub
                    auto cost = distance * u_cost + vessel_class->base_cost * (data.num_times - 1 - departure_time);
                    create_edge(*p, PickupType::PICKUP, departure_time, *n_h2.port, PickupType::DELIVERY, data.num_times - 1, g, cost);
                }
            }
            
            // For delivery nodes:
            for(auto departure_time = delivery_departure_time; departure_time < latest_departure(data, p, n_h2.port, *vessel_class); ++departure_time) {
                auto falls_in_tw = false;
                for(const auto& tw : p->closing_time_windows) {
                    if(departure_time > tw.first && departure_time <= tw.second) {
                        falls_in_tw = true;
                        break;
                    }
                }
            
                if(falls_in_tw) {
                    continue;
                }
                
                for(const auto& sc : vessel_class->bunker_cost) {
                    auto distance = data.distances.at(std::make_pair(p, n_h2.port));
                    auto speed = sc.first, u_cost = sc.second;
                    auto arrival_time = departure_time + (int) ceil(distance / speed);
                    
                    if(arrival_time >= data.num_times) {
                        continue;
                    }
                    
                    // Thew following works the same for:
                    // 1) When arrival_time == data.num_times - 1 => arrival at exact time => no waiting at hub
                    // 2) When arrival_time < data.num_times - 1 => early arrival => waiting at the hub
                    auto cost = distance * u_cost + vessel_class->base_cost * (data.num_times - 1 - departure_time);
                    create_edge(*p, PickupType::DELIVERY, departure_time, *n_h2.port, PickupType::DELIVERY, data.num_times - 1, g, cost);
                }
            }
        }
                
        /*  Add port-to-port edges */
        for(auto p : data.ports) {
            if(p->hub) {
                continue;
            }
        
            if(!p->allowed[vessel_class]) {
                continue;
            }
        
            for(auto t = earliest_arrival(data, p, n_h1.port, *vessel_class); t <= latest_departure(data, p, n_h2.port, *vessel_class); t++) {            
                auto falls_in_tw = false;
                for(const auto& tw : p->closing_time_windows) {
                    if(t > tw.first && t <= tw.second) {
                        falls_in_tw = true;
                        break;
                    }
                }
            
                if(falls_in_tw) {
                    continue;
                }
            
                for(auto pu : {PickupType::PICKUP, PickupType::DELIVERY}) {
                    if(pu == PickupType::PICKUP && t < data.num_times - 1 - p->pickup_transit) {
                        continue;
                    }
                    if(pu == PickupType::DELIVERY && t > p->delivery_transit) {
                        continue;
                    }
                
                    for(auto q : data.ports) {
                        if(p == q) {
                            continue;
                        }
                    
                        if(q->hub) {
                            continue;
                        }
                    
                        if(!q->allowed[vessel_class]) {
                            continue;
                        }
                    
                        for(const auto& sc : vessel_class->bunker_cost) {
                            auto distance = data.distances.at(std::make_pair(p, q));
                            auto speed = sc.first, u_cost = sc.second;
                            auto arrival_time = t + (int) ceil(distance / speed);
                        
                            if(arrival_time >= data.num_times) {
                                continue;
                            }
                        
                            if(arrival_time < earliest_arrival(data, p, n_h1.port, *vessel_class)) {
                                continue;
                            }
                            
                            if( (pu == PickupType::DELIVERY) ||
                                (pu == PickupType::PICKUP && p->pickup_demand + q->pickup_demand <= vessel_class->capacity)) {

                                auto final_time_pu = final_time(data, *q, arrival_time, PickupType::PICKUP);
                        
                                if( (final_time_pu <= latest_departure(data, q, n_h2.port, *vessel_class)) &&
                                    (final_time_pu >= data.num_times - 1 - q->pickup_transit)) {
                            
                                    auto cost_pu = distance * u_cost + vessel_class->base_cost * (final_time_pu - t) + q->fee[vessel_class];
                            
                                    create_edge(*p, pu, t, *q, PickupType::PICKUP, final_time_pu, g, cost_pu);
                                }
                            }
                        
                            if( (pu == PickupType::DELIVERY && p->delivery_demand + q->delivery_demand <= vessel_class->capacity) ||
                                (pu == PickupType::PICKUP && p->pickup_demand + q->delivery_demand <= vessel_class->capacity)) {

                                auto final_time_de = final_time(data, *q, arrival_time, PickupType::DELIVERY);
                        
                                if( (final_time_de <= latest_departure(data, q, n_h2.port, *vessel_class)) &&
                                    (final_time_de <= q->delivery_transit)) {
                            
                                    auto cost_de = distance * u_cost + vessel_class->base_cost * (final_time_de - t) + q->fee[vessel_class];
                                                    
                                    create_edge(*p, pu, t, *q, PickupType::DELIVERY, final_time_de, g, cost_de);
                                }
                            }
                        }
                    }
                }
            }
        }
            
        /*  Add delivery-to-pickup edges */
        for(auto vp = vertices(g->graph); vp.first != vp.second; ++vp.first) {
            auto n = *g->graph[*vp.first];
            if(n.n_type == NodeType::REGULAR_PORT && n.pu_type == PickupType::DELIVERY) {
                create_edge(*(n.port), n.pu_type, n.time_step, *(n.port), PickupType::PICKUP, n.time_step, g, 0);
            }
        }
    
        /*  Do some spring cleaning */
        auto clean = false;
        while(!clean) {
            clean = true;
            vit vi, vi_end, vi_next;
            std::tie(vi, vi_end) = vertices(g->graph);
            for(vi_next = vi; vi != vi_end; vi = vi_next) {
                ++vi_next;
                auto n_out = (int)out_degree(*vi, g->graph);
                auto n_in = (int)in_degree(*vi, g->graph);
                if((g->graph[*vi]->n_type == NodeType::REGULAR_PORT) && (n_out == 0 || n_in == 0 || n_out + n_in <= 1)) {
                    clear_vertex(*vi, g->graph);
                    remove_vertex(*vi, g->graph);
                    clean = false;
                }
            }
        }
        
        vit vi, vi_end;
        for(std::tie(vi, vi_end) = vertices(g->graph); vi != vi_end; ++vi) {
            oeit ei, ei_end, ei_next;
            std::tie(ei, ei_end) = out_edges(*vi, g->graph);
            
            std::unordered_map<Vertex, Edge> best_edges;
            for(ei_next = ei; ei != ei_end; ei = ei_next) {
                ++ei_next;
                
                auto destination = target(*ei, g->graph);
                auto cost = g->graph[*ei]->cost;
                
                if(best_edges.find(destination) == best_edges.end()) {
                    best_edges.emplace(destination, *ei);
                } else {
                    if(g->graph[best_edges[destination]]->cost > cost) {
                        auto old_edge = best_edges[destination];
                        best_edges[destination] = *ei;
                        remove_edge(old_edge, g->graph);
                    } else {
                        remove_edge(*ei, g->graph);
                    }
                }
            }
        }
    
        g->prepare_for_labelling();
    
        return g;
    }

    int final_time(const ProblemData& data, const Port& p, int arrival_time, PickupType pu) {
        auto ft_handling = arrival_time + (pu == PickupType::PICKUP ? p.pickup_handling : p.delivery_handling);
        auto ft = ft_handling;
        auto free_from_tw = false;
        
        while(!free_from_tw) {
            free_from_tw = true;
            for(const auto& tw : p.closing_time_windows) {
                auto t1 = tw.first, t2 = tw.second;
                if(ft <= t2 && ft > t1) {
                    ft += (t2 - t1);
                    free_from_tw = false;
                }
            }
        }
    
        return std::min(ft, data.num_times - 1);
    }

    int latest_departure(const ProblemData& data, std::shared_ptr<Port> p, std::shared_ptr<Port> h2, const VesselClass& vessel_class) {
        auto top_speed = vessel_class.top_speed;
        return std::max(0.0, data.num_times - floor(data.distances.at(std::make_pair(p, h2)) / top_speed));
    }

    int earliest_arrival(const ProblemData& data, std::shared_ptr<Port> p, std::shared_ptr<Port> h1, const VesselClass& vessel_class) {
        auto top_speed = vessel_class.top_speed;
        return std::min(data.num_times - 1.0, ceil(data.distances.at(std::make_pair(h1, p)) / top_speed));
    }

    void create_edge(const Port& origin_p, PickupType origin_pu, int origin_type,
                     const Port& destination_p, PickupType destination_pu, int destination_type,
                     std::shared_ptr<Graph> g, double cost) {
        bool origin_found, destination_found;
        Vertex origin_v, destination_v;

        std::tie(origin_found, origin_v) = g->get_vertex(origin_p, origin_pu, origin_type);
    
        if(!origin_found) {
            throw std::runtime_error("Can't find the origin vertex: " + origin_p.name + " at time " + std::to_string(origin_type));
        }
    
        std::tie(destination_found, destination_v) = g->get_vertex(destination_p, destination_pu, destination_type);
    
        if(!destination_found) {
            throw std::runtime_error("Can't find the destination vertex: " + destination_p.name + " at time " + std::to_string(destination_type));
        }
    
        Edge e = add_edge(origin_v, destination_v, g->graph).first;
        g->graph[e] = std::make_shared<Arc>(cost);
    }
}