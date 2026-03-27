<div align="center">
  <h1>🚀 BPSNDB</h1>
  <p><strong>A High-Performance Distributed B+ Tree Database</strong></p>
  <p><i>C++ Storage Engine | Go Orchestrator | gRPC Communication</i></p>
</div>

<hr />

<h2>📖 Overview</h2>
<p>
  BPSNDB is a distributed key-value storage system designed for high availability and read scalability. It utilizes a custom-built <b>C++ B+ Tree</b> for disk-persistent storage and a <b>Go Orchestrator</b> to manage a Primary-Secondary replication cluster.
</p>

<h2>🌟 Key Features</h2>
<ul>
  <li><b>Custom B+ Tree:</b> Low-level C++ implementation featuring node splitting, merging, and binary persistence.</li>
  <li><b>Read-Write Splitting:</b> All writes are routed to the Primary node, while reads are load-balanced across multiple Replicas using Round-Robin.</li>
  <li><b>gRPC Framework:</b> Efficient, language-agnostic communication between Go and C++ using Protocol Buffers.</li>
  <li><b>Automated Replication:</b> Go-driven asynchronous broadcast of write commands ensures eventual consistency across the cluster.</li>
  <li><b>Cold Boot Sync:</b> Intelligent shell-level synchronization that mirrors Primary data to Replicas on startup.</li>
</ul>

<h2>🛠 Tech Stack</h2>
<table width="100%">
  <tr>
    <td width="50%">
      <b>Backend Engine</b>
      <ul>
        <li>C++17</li>
        <li>gRPC (C++)</li>
        <li>Protobuf</li>
      </ul>
    </td>
    <td width="50%">
      <b>Orchestrator</b>
      <ul>
        <li>Go (Golang)</li>
        <li>gRPC (Go)</li>
        <li>Standard SQL-like Parser</li>
      </ul>
    </td>
  </tr>
</table>

<hr />

<h2>🚀 Getting Started</h2>

<h3>Prerequisites</h3>
<pre>
# Install gRPC and Protobuf (macOS)
brew install grpc protobuf pkg-config

# Prepare Go dependencies
go mod tidy
</pre>

<h3>Running the Cluster</h3>
<p>The system includes a <code>run_cluster.sh</code> script that handles compilation, synchronization, and node launching in one command:</p>
<pre>
chmod +x run_cluster.sh
./run_cluster.sh
</pre>

<hr />

<h2>💻 Database Console</h2>
<p>Once the orchestrator is running, use the built-in CLI to interact with the distributed system:</p>
<table border="1">
  <thead>
    <tr>
      <th>Command</th>
      <th>Example</th>
      <th>Internal Path</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td><b>INSERT</b></td>
      <td><code>insert 1 bhanu b@mail.com</code></td>
      <td>Primary &rarr; Replicas</td>
    </tr>
    <tr>
      <td><b>SELECT</b></td>
      <td><code>select</code></td>
      <td>Replica (Load Balanced)</td>
    </tr>
    <tr>
      <td><b>UPDATE</b></td>
      <td><code>update 1 bhanu new@mail.com</code></td>
      <td>Primary &rarr; Replicas</td>
    </tr>
    <tr>
      <td><b>DELETE</b></td>
      <td><code>delete 1</code></td>
      <td>Primary &rarr; Replicas</td>
    </tr>
  </tbody>
</table>

<hr />

<h2>📁 Project Structure</h2>
<ul>
  <li><code>/db-engine</code>: C++ B+ Tree source code and gRPC Server.</li>
  <li><code>/proto</code>: Service definitions (.proto files).</li>
  <li><code>main.go</code>: Go Orchestrator and Cluster Controller.</li>
  <li><code>run_cluster.sh</code>: Master automation script.</li>
</ul>

<div align="center">
  <p>Built by Bhanu Negi BPSN</p>
</div>
