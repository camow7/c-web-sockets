#include "client_ws.hpp"
#include "server_ws.hpp"

using namespace std;

using WsServer = SimpleWeb::SocketServer<SimpleWeb::WS>;
using WsClient = SimpleWeb::SocketClient<SimpleWeb::WS>;

int main() {
  // WebSocket (WS)-server at port 8080 using 1 thread
  WsServer server;
  server.config.port = 2506;

  // Example 1: echo WebSocket endpoint
  // Added debug messages for example use of the callbacks
  // Test with the following JavaScript:
  //   var ws=new WebSocket("ws://localhost:8080/echo");
  //   ws.onmessage=function(evt){console.log(evt.data);};
  //   ws.send("test");
  auto &echo = server.endpoint["^/echo/?$"];

  echo.on_message = [](shared_ptr<WsServer::Connection> connection, shared_ptr<WsServer::Message> message) {
    auto message_str = message->string();

    cout << "Server: Message received: \"" << message_str << "\" from " << connection.get() << endl;

    cout << "Server: Sending message \"" << message_str << "\" to " << connection.get() << endl;

    auto send_stream = make_shared<WsServer::SendStream>();
    *send_stream << message_str;
    // connection->send is an asynchronous function
    connection->send(send_stream, [](const SimpleWeb::error_code &ec) {
      if(ec) {
        cout << "Server: Error sending message. " <<
            // See http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/reference.html, Error Codes for error code meanings
            "Error: " << ec << ", error message: " << ec.message() << endl;
      }
    });
  };

  echo.on_open = [](shared_ptr<WsServer::Connection> connection) {
    cout << "Server: Opened connection " << connection.get() << endl;
  };

  // See RFC 6455 7.4.1. for status codes
  echo.on_close = [](shared_ptr<WsServer::Connection> connection, int status, const string & /*reason*/) {
    cout << "Server: Closed connection " << connection.get() << " with status code " << status << endl;
  };

  // See http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/reference.html, Error Codes for error code meanings
  echo.on_error = [](shared_ptr<WsServer::Connection> connection, const SimpleWeb::error_code &ec) {
    cout << "Server: Error in connection " << connection.get() << ". "
         << "Error: " << ec << ", error message: " << ec.message() << endl;
  };

  // Example 2: Echo thrice
  // Demonstrating queuing of messages by sending a received message three times back to the client.
  // Concurrent send operations are automatically queued by the library.
  // Test with the following JavaScript:
  //   var ws=new WebSocket("ws://localhost:8080/echo_thrice");
  //   ws.onmessage=function(evt){console.log(evt.data);};
  //   ws.send("test");
  auto &echo_thrice = server.endpoint["^/echo_thrice/?$"];
  echo_thrice.on_message = [](shared_ptr<WsServer::Connection> connection, shared_ptr<WsServer::Message> message) {
    auto send_stream = make_shared<WsServer::SendStream>();
    *send_stream << message->string();

    connection->send(send_stream, [connection, send_stream](const SimpleWeb::error_code &ec) {
      if(!ec)
        connection->send(send_stream); // Sent after the first send operation is finished
    });
    connection->send(send_stream); // Most likely queued. Sent after the first send operation is finished.
  };

  // Example 3: Echo to all WebSocket endpoints
  // Sending received messages to all connected clients
  // Test with the following JavaScript on more than one browser windows:
  //   var ws=new WebSocket("ws://localhost:8080/echo_all");
  //   ws.onmessage=function(evt){console.log(evt.data);};
  //   ws.send("test");
  auto &echo_all = server.endpoint["^/echo_all/?$"];
  echo_all.on_message = [&server](shared_ptr<WsServer::Connection> /*connection*/, shared_ptr<WsServer::Message> message) {
    auto send_stream = make_shared<WsServer::SendStream>();
    *send_stream << message->string();

    // echo_all.get_connections() can also be used to solely receive connections on this endpoint
    for(auto &a_connection : server.get_connections())
      a_connection->send(send_stream);
  };

  thread server_thread([&server]() {
    // Start WS-server
    server.start();
  });

  // Wait for server to start so that the client can connect
  this_thread::sleep_for(chrono::seconds(1));

  // Example 4: Client communication with server
  // Possible output:
  //   Server: Opened connection 0x7fcf21600380
  //   Client: Opened connection
  //   Client: Sending message: "Hello"
  //   Server: Message received: "Hello" from 0x7fcf21600380
  //   Server: Sending message "Hello" to 0x7fcf21600380
  //   Client: Message received: "Hello"
  //   Client: Sending close connection
  //   Server: Closed connection 0x7fcf21600380 with status code 1000
  //   Client: Closed connection with status code 1000
  WsClient client("localhost:8080/echo");
  client.on_message = [](shared_ptr<WsClient::Connection> connection, shared_ptr<WsClient::Message> message) {
    cout << "Client: Message received: \"" << message->string() << "\"" << endl;

    cout << "Client: Sending close connection" << endl;
    connection->send_close(1000);
  };

  client.on_open = [](shared_ptr<WsClient::Connection> connection) {
    cout << "Client: Opened connection" << endl;

    string message = "Hello";
    cout << "Client: Sending message: \"" << message << "\"" << endl;

    auto send_stream = make_shared<WsClient::SendStream>();
    *send_stream << message;
    connection->send(send_stream);
  };

  client.on_close = [](shared_ptr<WsClient::Connection> /*connection*/, int status, const string & /*reason*/) {
    cout << "Client: Closed connection with status code " << status << endl;
  };

  // See http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/reference.html, Error Codes for error code meanings
  client.on_error = [](shared_ptr<WsClient::Connection> /*connection*/, const SimpleWeb::error_code &ec) {
    cout << "Client: Error: " << ec << ", error message: " << ec.message() << endl;
  };

  client.start();

  server_thread.join();
}
