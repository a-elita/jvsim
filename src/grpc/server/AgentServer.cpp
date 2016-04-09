//
//  AgentServer.cpp
//  agent-jv
//
//  Created by NITIN KUMAR on 12/29/15.
//  Copyright © 2015 Juniper Networks. All rights reserved.
//

// Header files
#include <google/protobuf/text_format.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include "AgentServer.h"
#include "OpenConfig.hpp"

Status
AgentServer::telemetrySubscribe (ServerContext *context,
                                 const SubscriptionRequest *request,
                                 ServerWriter<OpenConfigData>* writer)
{
    PathList    path_list;
    std::string log_str;
    AgentConsolidatorHandle *system_handle;
    SubscriptionReply reply;
    SubscriptionResponse *response = reply.mutable_response();

    // Get all the paths that we are interested in
    for (int i = 0; i < request->path_list_size(); i++) {
        path_list.push_back(request->path_list(i).path());
        log_str += request->path_list(i).path();
        log_str += ":";
    }

    // Make a note
    _logger->log("Subscription-Begin: " + log_str);

    // Allocate an ID
    id_idx_t id = _id_manager.allocate();
    if (id == _id_manager.getNullIdentifier()) {
        // Sent id error message and bail out.
        response->set_subscription_id(id);
        _logger->log("Subscription-stream-end: Error, ID Exhausted");
        return _sendMetaDataInfo(context, writer, reply);
    }

    // Create a subscription into the system
    system_handle = _consolidator.addRequest(std::to_string(id), request);
    if (!system_handle) {
        // TODO ABBAS should we delete allocated id ?
        // Sent id = 0 error message and bail out.
        response->set_subscription_id(_id_manager.getNullIdentifier());
        _logger->log("Subscription-stream-end: Error, Internal System Failure");
        return _sendMetaDataInfo(context, writer, reply);
    }

    // Create a subscription
    AgentServerTransport transport(context, writer);
    AgentSubscriptionLimits limits(request->additional_config().limit_records(), request->additional_config().limit_time_seconds());
    AgentSubscription *sub = AgentSubscription::createSubscription(id,
                                                                   system_handle,
                                                                   transport,
                                                                   path_list,
                                                                   limits);
    if (!sub) {
        // TODO ABBAS should we delete allocated id ?
        // Sent id = 0 error message and bail out.
        response->set_subscription_id(_id_manager.getNullIdentifier());
        _logger->log("Subscription-stream-end: Error, Subscription Creation Error");
        return _sendMetaDataInfo(context, writer, reply);
    }

    // Log the Subscription Identifier
    log_str = std::to_string(sub->getId());
    _logger->log("Subscription-Allocate: ID = " + log_str);

    // Send back the response on the metadata channel
    response->set_subscription_id(sub->getId());

#if 0 /* TODO ABBAS */
    context->AddInitialMetadata("subscription-name", sub->getName());
    for (int i = 0; i < request->path_list_size(); i++) {
        std::string key_str = "path-" + std::to_string(i);
        context->AddInitialMetadata(key_str, request->path_list(i).path());
    }
    context->AddInitialMetadata("limit_records", std::to_string(limits.getRecords()));
    context->AddInitialMetadata("limit_seconds", std::to_string(limits.getSeconds()));
#else
    PathList pList = sub->getPathList();
    for (PathList::iterator it = pList.begin(); it < pList.end(); it++) {
        Telemetry::Path *p_tpath = reply.add_path_list();
        p_tpath->set_path(*it);
    }
    // Don't start MQTT loop till we send this response
    // TODO ABBAS
    _sendMetaDataInfo(context, writer, reply);
#endif

    // Turn it on
    sub->enable();

    bool client_disconnected = false;
    // Wait till the subscription gets cancelled or it expires
    while (!sub->expired() && sub->getActive()) {
        if (sub->getTransport().getServerContext()->IsCancelled() == true) {
            // Client is disconnected
            client_disconnected = true;
            sub->setActive(false);
        }
        sleep(1);
    }

    // Streaming over. The subscription will be deleted by unSubscribe
    if (client_disconnected) {
        // cleanup subscription gracefully
        _cleanupSubscription(sub);
    }
    delete sub;

    log_str = std::to_string(sub->getId());
    _logger->log("Subscription-stream-end: ID = " + log_str);

    return Status::OK;
}


Status
AgentServer::cancelTelemetrySubscription (ServerContext* context, const CancelSubscriptionRequest* cancel_request,CancelSubscriptionReply* cancel_reply)
{
    // Make a note
    _logger->log("cancelTelemetrySubscription: ID = " + std::to_string(cancel_request->subscription_id()));

    // Lookup the subscription
    AgentSubscription *sub = AgentSubscription::findSubscription(cancel_request->subscription_id());
    if (!sub) {
        std::string err_str = "Subscription Not Found = " + std::to_string(cancel_request->subscription_id());
        cancel_reply->set_code(Telemetry::NO_SUBSCRIPTION_ENTRY);
        cancel_reply->set_code_str(err_str);
        _logger->log(err_str);
        return Status::OK;
    }

    // cleanup subscription gracefully
    _cleanupSubscription(sub);
    
    // TODO ABBAS do not delete here, leave it to subscription thread
    // delete sub;

    cancel_reply->set_code(Telemetry::SUCCESS);
    cancel_reply->set_code_str("Subscription Successfully Deleted");
    _logger->log("cancelTelemetrySubscriptionEnd: ID = " + std::to_string(cancel_request->subscription_id()));

    return Status::OK;
}


Status
AgentServer::getTelemetrySubscriptions (ServerContext* context, const GetSubscriptionsRequest* get_request, GetSubscriptionsReply* get_reply)
// AgentServer::telemetrySubscriptionsGet (ServerContext *context, const agent::GetRequest *args, agent::OpenConfigData *datap)
{
#if 0
    AgentSubscription *sub;

    // Fill in the base
    // Fill in the common header
    datap->set_system_id("1.1.1.1");
    datap->set_component_id(1);
    datap->set_timestamp(100);

    // Iterate the subscription data base
    sub = AgentSubscription::getFirst();
    while (sub) {
        // Add a new entry
        KeyValue *kv;
        kv = datap->add_kv();
        kv->set_key(sub->getName());
        kv->set_int_value(sub->getId());
        
        // Move to the next entry
        sub = AgentSubscription::getNext(sub->getId());
    }
#endif
    AgentSubscription *sub;

    // Iterate the subscription data base
    sub = AgentSubscription::getFirst();
    while (sub) {
        // Add a new entry
        SubscriptionReply *sub_reply;
        sub_reply = get_reply->add_subscription_list();
        SubscriptionResponse sub_resp;
        sub_resp.set_subscription_id(sub->getId());
        sub_reply->set_allocated_response(&sub_resp);

        PathList pathList = sub->getPathList();

        for (PathList::iterator it = pathList.begin() ; it != pathList.end(); ++it) {
            Telemetry::Path *path = sub_reply->add_path_list();
            path->set_path(*it);
        }

        // Move to the next entry
        sub = AgentSubscription::getNext(sub->getId());
    }

    return Status::OK;
}


Status
AgentServer::getTelemetryOperationalState (ServerContext* context, const GetOperationalStateRequest* operational_request, GetOperationalStateReply* operational_reply)
{
    // TODO ABBAS --- fix me
    OpenConfigData *datap = new OpenConfigData();

    // Make a note
    _logger->log("GetOperationalState:ID = " + std::to_string(operational_request->subscription_id()));

    // If a subscription ID hasn't been set, then we are done
    if (!operational_request->subscription_id()) {
        return Status::OK;
    }

    // Lookup the subscription
    AgentSubscription *sub = AgentSubscription::findSubscription(operational_request->subscription_id());
    if (!sub) {
        std::string err_str = "Subscription Not Found=" + std::to_string(operational_request->subscription_id());
        _logger->log(err_str);
        return Status::OK;
    }

    // Get all the statistics for this subscription
    uint32_t verbosity = operational_request->verbosity() ? operational_request->verbosity() : 0;
    sub->getOperational(datap, verbosity);

    return Status::OK;
}


Status
AgentServer::getDataEncodings (ServerContext* context, const DataEncodingRequest* data_enc_request, DataEncodingReply* data_enc_reply)
{
    data_enc_reply->set_encoding_list(0, ::Telemetry::PROTO3);
    
    return Status::OK;
}


Status
AgentServer::_sendMetaDataInfo (ServerContext *context,
                                ServerWriter<OpenConfigData>* writer,
                                SubscriptionReply &reply)
{
    // Serialize the data in text format
    std::string init_data;
    google::protobuf::TextFormat::Printer printer;
    // Use single line mode
    printer.SetSingleLineMode(true);
    printer.PrintToString(reply, &init_data);
    std::cout << "String data = " << init_data << std::endl;

    // Send the meta data and log it
    context->AddInitialMetadata("init-response", init_data);
    writer->SendInitialMetadata();
    _logger->log("Subscribe-MetaData-Sent");
    return Status::OK;
}


void
AgentServer::_cleanupSubscription (AgentSubscription *sub)
{
    if (sub == NULL) {
        // Bail out
        return;
    }

    // Cancel the subscription on the bus
    sub->disable();
    
    // Disconnect and stop the MQTT loop
    sub->disconnect();
    sub->loop_stop();
    
    // Remove the subscription from the system
    _consolidator.removeRequest(sub->getSystemSubscription());
    
    // Remove the subscription
    AgentSubscription::deleteSubscription(sub->getId());
    _id_manager.deallocate(sub->getId());
}