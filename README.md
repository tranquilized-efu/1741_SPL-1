Load Balancer in C++

* Phase 1 — Networking fundamentals
Days 1–2
   * Understand TCP sockets.
   * Implement a basic client.
   * Implement one backend server.
   * Establish successful communication.
Deliverable: Client sends a message and receives a response from a backend server.
* Phase 2 — Multiple backend servers
Days 3–4
   * Run three backend server instances.
   * Assign different ports.
   * Implement the Server structure.
   * Build the Server Manager.
Deliverable: Three backend servers running simultaneously.
* Phase 3 — Load balancing algorithms
Days 5–7
   * Implement Round Robin.
   * Implement Least Connections.
   * Implement IP Hash.
   * Add algorithm selection through the main controller.
Deliverable: Requests are distributed using all three algorithms.
* Phase 4 — Client request management
Days 8–9
   * Accept multiple clients.
   * Implement request forwarding.
   * Return backend responses.
   * Add connection tracking and thread synchronization.
Deliverable: Multiple clients can communicate with backend servers through the load balancer.
* Phase 5 — Health monitoring
Days 10–11
   * Implement periodic health checks.
   * Track server failures.
   * Automatically exclude unhealthy servers.
   * Implement server recovery.
Deliverable: Load balancer automatically avoids unavailable servers.
* Phase 6 — Failure simulation
Days 12–13
   * Implement administrative controls.
   * Simulate backend failures.
   * Restore failed servers.
   * Test all-server failure.
Deliverable: Demonstrate fault detection and recovery.
* Phase 7 — Performance comparison
Days 14–16
   * Generate multiple requests.
   * Measure response times.
   * Calculate throughput.
   * Compare server distribution.
   * Generate performance reports.
Deliverable: A comparative report for all three algorithms.
* Phase 8 — Testing and final integration
Days 17–18
   * Fix concurrency issues.
   * Test edge cases.
   * Verify LOC.
   * Prepare project documentation.
   * Prepare demonstration scenarios.
Deliverable: A complete working project.
